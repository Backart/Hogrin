#include "bootstrap_client.h"

Bootstrap_Client::Bootstrap_Client(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnect_timer(new QTimer(this))
{
    m_reconnect_timer->setSingleShot(true);
    m_reconnect_timer->setInterval(2000); // retry after 2s

    connect(m_socket, &QTcpSocket::connected,
            this, &Bootstrap_Client::on_connected);

    connect(m_socket, &QTcpSocket::disconnected,
            this, &Bootstrap_Client::on_disconnected);

    connect(m_socket, &QTcpSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError err){
                qWarning() << "Bootstrap socket error:" << err << m_socket->errorString();
                m_socket->abort();
                if (!m_queue.isEmpty()) m_reconnect_timer->start();
            });

    connect(m_socket, &QTcpSocket::readyRead, this, [this](){
        m_last_response_time = QDateTime::currentDateTime();
        while (m_socket->canReadLine()) {
            QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
            if (!line.isEmpty())
                parse_response(line);
        }
    });

    connect(m_reconnect_timer, &QTimer::timeout,
            this, &Bootstrap_Client::ensure_connected);
}

// ── connection management ─────────────────────────────────────────────────────

void Bootstrap_Client::ensure_connected()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState ||
        m_socket->state() == QAbstractSocket::ConnectingState)
        return;

    qDebug() << "Bootstrap: connecting to"
             << Config::BOOTSTRAP_SERVER << Config::BOOTSTRAP_PORT;
    m_socket->connectToHost(QString(Config::BOOTSTRAP_SERVER),
                            Config::BOOTSTRAP_PORT);
}

void Bootstrap_Client::on_connected()
{
    qDebug() << "Bootstrap: connected";
    m_last_response_time = QDateTime::currentDateTime();
    m_reconnect_timer->stop();
    flush_queue();
}

void Bootstrap_Client::on_disconnected()
{
    qDebug() << "Bootstrap: disconnected — retrying in 2s";
    if (!m_queue.isEmpty())
        m_reconnect_timer->start();
}

void Bootstrap_Client::enqueue(const QString &message)
{
    bool was_empty = m_queue.isEmpty();
    m_queue.enqueue(message);

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        if (was_empty && m_last_response_time.isValid() && m_last_response_time.msecsTo(QDateTime::currentDateTime()) > 15000) {
            qWarning() << "Zombie socket detected (idle > 4s). Force reconnecting...";
            m_socket->abort();
            ensure_connected();
        } else {
            m_last_response_time = QDateTime::currentDateTime();
            flush_queue();
        }
    } else {
        ensure_connected();
    }
}

void Bootstrap_Client::flush_queue()
{
    while (!m_queue.isEmpty() &&
           m_socket->state() == QAbstractSocket::ConnectedState)
    {
        QString msg = m_queue.dequeue();
        qDebug() << "Bootstrap send:" << msg.left(80);
        m_socket->write((msg + "\n").toUtf8());
        m_socket->flush();
    }
}

// ── response parsing ──────────────────────────────────────────────────────────

void Bootstrap_Client::parse_response(const QString &response)
{
    qDebug() << "Bootstrap response:" << response;

    if (response == "AUTH_REG_OK") {
        emit auth_register_success();
    }
    else if (response.startsWith("AUTH_REG_ERR:")) {
        emit auth_register_failed(response.mid(13));
    }
    else if (response.startsWith("AUTH_LOGIN_OK:")) {
        emit auth_login_success(response.mid(14), m_pending_auth_nickname);
    }
    else if (response.startsWith("AUTH_LOGIN_ERR:")) {
        emit auth_login_failed(response.mid(15));
    }
    else if (response.startsWith("AUTH_VERIFY_OK:")) {
        emit auth_verify_success(response.mid(15));
    }
    else if (response.startsWith("AUTH_VERIFY_ERR:")) {
        emit auth_verify_failed(response.mid(16));
    }
    else if (response == "AUTH_LOGOUT_OK") {
        emit auth_logout_success();
    }
    else if (response.startsWith("FOUND:")) {
        QStringList parts = response.mid(6).split("|");
        if (parts.size() == 3) {
            emit user_found(m_pending_find_nickname,
                            parts[0],
                            parts[1].toUShort(),
                            QByteArray::fromHex(parts[2].toUtf8()));
        }
    }
    else if (response == "NOT_FOUND") {
        emit user_not_found(m_pending_find_nickname);
    }
    else if (response == "OK") {
        emit store_confirmed();
    }
    else if (response.startsWith("ERROR:")) {
        emit store_failed(response.mid(6));
        qWarning() << "Relay store error:" << response.mid(6);
    }
    else if (response == "EMPTY") {
        emit messages_fetched({});
    }
    else if (response.startsWith("MSG:")) {
        QByteArray blob = QByteArray::fromHex(response.mid(4).toUtf8());
        if (!blob.isEmpty())
            emit messages_fetched({blob});
    }
    else if (response.startsWith("REGISTER_OK:")) {
        emit registered(response.mid(12));
    }
}

// ── public API ────────────────────────────────────────────────────────────────

void Bootstrap_Client::auth_register(const QString &nickname, const QString &password)
{
    qDebug() << "Auth register:" << nickname;
    enqueue(QString("AUTH_REGISTER:%1:%2").arg(nickname, password));
}

void Bootstrap_Client::auth_login(const QString &nickname, const QString &password)
{
    qDebug() << "Auth login:" << nickname;
    m_pending_auth_nickname = nickname;
    enqueue(QString("AUTH_LOGIN:%1:%2").arg(nickname, password));
}

void Bootstrap_Client::auth_verify(const QString &token)
{
    qDebug() << "Auth verify";
    enqueue(QString("AUTH_VERIFY:%1").arg(token));
}

void Bootstrap_Client::auth_logout(const QString &token)
{
    qDebug() << "Auth logout";
    enqueue(QString("AUTH_LOGOUT:%1").arg(token));
}

void Bootstrap_Client::register_user(const QString &nickname,
                                     quint16 port,
                                     const QByteArray &public_key)
{
    qDebug() << "Register user:" << nickname;
    enqueue(QString("REGISTER:%1:%2:%3")
                .arg(nickname)
                .arg(port)
                .arg(QString::fromUtf8(public_key.toHex())));
}

void Bootstrap_Client::find_user(const QString &nickname)
{
    qDebug() << "Find user:" << nickname;
    m_pending_find_nickname = nickname;
    enqueue(QString("FIND:%1").arg(nickname));
}

void Bootstrap_Client::unregister_user(const QString &nickname)
{
    qDebug() << "Unregister user:" << nickname;
    enqueue(QString("UNREGISTER:%1").arg(nickname));
}

bool Bootstrap_Client::store_message(const QString &nickname,
                                     const QByteArray &encrypted_blob)
{
    qDebug() << "Store relay message for:" << nickname;
    enqueue(QString("STORE:%1:%2")
                .arg(nickname)
                .arg(QString::fromUtf8(encrypted_blob.toHex())));
    return true;
}

void Bootstrap_Client::fetch_messages(const QString &nickname)
{
    enqueue(QString("FETCH:%1").arg(nickname));
}

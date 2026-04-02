#include "bootstrap_client.h"

Bootstrap_Client::Bootstrap_Client(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnect_timer(new QTimer(this))
    , m_ws_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_fallback_timer(new QTimer(this))
{
    m_reconnect_timer->setSingleShot(true);
    m_reconnect_timer->setInterval(2000); // retry after 2s

    m_fallback_timer->setSingleShot(true);
    m_fallback_timer->setInterval(3000); // 3s timeout for TCP fallback

    // ── TCP (IPv6) ────────────────────────────────────────────────────────────

    connect(m_socket, &QTcpSocket::connected, this, [this](){
        m_fallback_timer->stop();
        m_use_ws = false;
        on_connected();
    });

    connect(m_socket, &QTcpSocket::disconnected, this, &Bootstrap_Client::on_disconnected);

    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError err){
        qWarning() << "Bootstrap TCP error:" << err << m_socket->errorString();
    });

    connect(m_socket, &QTcpSocket::readyRead, this, [this](){
        m_last_response_time = QDateTime::currentDateTime();
        while (m_socket->canReadLine()) {
            QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
            if (!line.isEmpty()) parse_response(line);
        }
    });

    // ── WebSocket (IPv4 Fallback) ─────────────────────────────────────────────

    connect(m_ws_socket, &QWebSocket::connected, this, [this](){
        m_use_ws = true;
        on_connected();
    });

    connect(m_ws_socket, &QWebSocket::disconnected, this, &Bootstrap_Client::on_disconnected);

    connect(m_ws_socket, &QWebSocket::textMessageReceived, this, [this](const QString &msg){
        m_last_response_time = QDateTime::currentDateTime();
        parse_response(msg);
    });

    // ── Fallback Logic ────────────────────────────────────────────────────────

    connect(m_fallback_timer, &QTimer::timeout, this, [this](){
        if (m_socket->state() != QAbstractSocket::ConnectedState) {
            qDebug() << "Bootstrap: TCP timeout. Falling back to WebSocket (IPv4)...";
            m_socket->abort();

            // Assume wss:// scheme via Cloudflare
            QString ws_url = QString("wss://%1").arg(Config::BOOTSTRAP_SERVER);
            m_ws_socket->open(QUrl(ws_url));
        }
    });

#ifdef HAS_QT_NETWORK_INFORMATION
    if (QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability)) {
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged,
                this, [this](QNetworkInformation::Reachability r) {
                    if (r == QNetworkInformation::Reachability::Online) {
                        qDebug() << "Bootstrap: network restored — forcing reconnect";
                        force_reconnect();
                    }
                });
        qDebug() << "Bootstrap: network change detection active";
    }
#else
    qDebug() << "Bootstrap: network change detection not available (no Qt6::NetworkInformation)";
#endif

    connect(m_reconnect_timer, &QTimer::timeout, this, &Bootstrap_Client::ensure_connected);
}

// ── connection management ─────────────────────────────────────────────────────

void Bootstrap_Client::ensure_connected()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState ||
        m_socket->state() == QAbstractSocket::ConnectingState ||
        m_ws_socket->isValid())
    {
        return;
    }

    qDebug() << "Bootstrap: connecting TCP to" << Config::BOOTSTRAP_SERVER << Config::BOOTSTRAP_PORT;

    // Always attempt TCP first (fastest route for IPv6)
    m_socket->connectToHost(QString(Config::BOOTSTRAP_SERVER), Config::BOOTSTRAP_PORT);

    // Start fallback timer. If TCP fails or hangs, it triggers WS connection
    m_fallback_timer->start();
}

void Bootstrap_Client::on_connected()
{
    qDebug() << "Bootstrap: connected via" << (m_use_ws ? "WebSocket" : "TCP");
    m_last_response_time = QDateTime::currentDateTime();
    m_reconnect_timer->stop();

    if (!m_first_connect) {
        emit reconnected();
    }
    m_first_connect = false;

    flush_queue();
}

void Bootstrap_Client::on_disconnected()
{
    qDebug() << "Bootstrap: disconnected — retrying in 2s";
    m_first_connect = false;
    m_use_ws = false;
    m_reconnect_timer->start();
}

void Bootstrap_Client::enqueue(const QString &message)
{
    bool was_empty = m_queue.isEmpty();
    m_queue.enqueue(message);

    bool is_connected = m_use_ws ? m_ws_socket->isValid() : (m_socket->state() == QAbstractSocket::ConnectedState);

    if (is_connected) {
        if (was_empty && m_last_response_time.isValid()
            && m_last_response_time.msecsTo(QDateTime::currentDateTime()) > 6000) {
            qWarning() << "Zombie socket detected (idle > 6s). Force reconnecting...";
            QTimer::singleShot(0, this, [this](){ force_reconnect(); });
        } else {
            flush_queue();
        }
    } else {
        ensure_connected();
    }
}

void Bootstrap_Client::flush_queue()
{
    while (!m_queue.isEmpty())
    {
        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            QString msg = m_queue.dequeue();
            qDebug() << "Bootstrap send (TCP):" << msg.left(80);
            m_socket->write((msg + "\n").toUtf8());
            m_socket->flush();
        } else if (m_ws_socket->isValid()) {
            QString msg = m_queue.dequeue();
            qDebug() << "Bootstrap send (WS):" << msg.left(80);
            m_ws_socket->sendTextMessage(msg);
        } else {
            break; // Connection lost while flushing
        }
    }
}

void Bootstrap_Client::force_reconnect()
{
    m_fallback_timer->stop();
    m_socket->abort();
    m_ws_socket->abort();
    m_use_ws = false;
    QTimer::singleShot(500, this, &Bootstrap_Client::ensure_connected);
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
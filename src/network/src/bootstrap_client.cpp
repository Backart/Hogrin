#include "bootstrap_client.h"
#include "../../common/config.h"
#include <QDebug>

Bootstrap_Client::Bootstrap_Client(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, [this](){
        while (m_socket->canReadLine()) {
            QString response = QString::fromUtf8(m_socket->readLine()).trimmed();
            if (response.isEmpty()) continue;

            qDebug() << "Bootstrap response:" << response;

            if (response == "AUTH_REG_OK") {
                emit auth_register_success();
            }
            else if (response.startsWith("AUTH_REG_ERR:")) {
                emit auth_register_failed(response.mid(13));
            }
            else if (response.startsWith("AUTH_LOGIN_OK:")) {
                QString token = response.mid(14);
                emit auth_login_success(token, m_pending_auth_nickname);
            }
            else if (response.startsWith("AUTH_LOGIN_ERR:")) {
                emit auth_login_failed(response.mid(15));
            }
            else if (response.startsWith("AUTH_VERIFY_OK:")) {
                QString nickname = response.mid(15);
                emit auth_verify_success(nickname);
            }
            else if (response.startsWith("AUTH_VERIFY_ERR:")) {
                emit auth_verify_failed(response.mid(16));
            }
            else if (response == "AUTH_LOGOUT_OK") {
                emit auth_logout_success();
            }
            else if (response.startsWith("FOUND:")) {
                QString data = response.mid(6);
                QStringList parts = data.split("|");
                if (parts.size() == 3) {
                    QString    host    = parts[0];
                    quint16    port    = parts[1].toUShort();
                    QByteArray pub_key = QByteArray::fromHex(parts[2].toUtf8());
                    emit user_found(m_pending_find_nickname, host, port, pub_key);
                }
            }
            else if (response == "NOT_FOUND") {
                emit user_not_found("");
            }
            else if (response == "OK") {
                emit store_confirmed();
            }
            else if (response.startsWith("ERROR:")) {
                QString reason = response.mid(6);
                emit store_failed(reason);
                qWarning() << "Relay store error:" << reason;
            }
            else if (response == "EMPTY") {
                emit messages_fetched({});
            }
            else if (response.startsWith("MSG:")) {
                QByteArray blob = QByteArray::fromHex(response.mid(4).toUtf8());
                if (!blob.isEmpty()) {
                    emit messages_fetched({blob});
                }
            }
            else if (response.startsWith("REGISTER_OK:")) {
                QString public_ip = response.mid(12);
                emit registered(public_ip);
            }
        }
    });
}

void Bootstrap_Client::connect_and_send(const QString &message)
{
    m_socket->abort();

    connect(m_socket, &QTcpSocket::connected, this, [this, message](){
        qDebug() << "Connected to bootstrap, sending:" << message;
        m_socket->write((message + "\n").toUtf8());
        m_socket->flush();
    }, Qt::SingleShotConnection);

    connect(m_socket, &QTcpSocket::errorOccurred, this, [](QAbstractSocket::SocketError err){
        qDebug() << "Bootstrap connection error:" << err;
    }, Qt::SingleShotConnection);

    qDebug() << "Connecting to bootstrap:" << Config::BOOTSTRAP_SERVER << Config::BOOTSTRAP_PORT;
    m_socket->connectToHost(QString(Config::BOOTSTRAP_SERVER), Config::BOOTSTRAP_PORT);
}

void Bootstrap_Client::auth_register(const QString &nickname,
                                     const QString &password)
{
    connect_and_send(QString("AUTH_REGISTER:%1:%2").arg(nickname, password));
    qDebug() << "Registering auth on bootstrap:" << nickname;
}

void Bootstrap_Client::auth_login(const QString &nickname,
                                  const QString &password)
{
    m_pending_auth_nickname = nickname; // Запам'ятовуємо нік для сигналу успіху
    connect_and_send(QString("AUTH_LOGIN:%1:%2").arg(nickname, password));
    qDebug() << "Logging in on bootstrap:" << nickname;
}

void Bootstrap_Client::auth_verify(const QString &token)
{
    connect_and_send(QString("AUTH_VERIFY:%1").arg(token));
    qDebug() << "Verifying session token on bootstrap";
}

void Bootstrap_Client::auth_logout(const QString &token)
{
    connect_and_send(QString("AUTH_LOGOUT:%1").arg(token));
    qDebug() << "Logging out on bootstrap";
}

void Bootstrap_Client::register_user(const QString &nickname,
                                     quint16 port,
                                     const QByteArray &public_key)
{
    QString pubkey_hex = QString::fromUtf8(public_key.toHex());
    connect_and_send(QString("REGISTER:%1:%2:%3")
                         .arg(nickname)
                         .arg(port)
                         .arg(pubkey_hex));
    qDebug() << "Registering on bootstrap:" << nickname;
}

void Bootstrap_Client::find_user(const QString &nickname)
{
    m_pending_find_nickname = nickname;
    connect_and_send(QString("FIND:%1").arg(nickname));
    qDebug() << "Finding user:" << nickname;
}

void Bootstrap_Client::unregister_user(const QString &nickname)
{
    connect_and_send(QString("UNREGISTER:%1").arg(nickname));
    qDebug() << "Unregistering from bootstrap:" << nickname;
}

bool Bootstrap_Client::store_message(const QString &nickname,
                                     const QByteArray &encrypted_blob)
{
    QString hex = QString::fromUtf8(encrypted_blob.toHex());
    connect_and_send(QString("STORE:%1:%2").arg(nickname).arg(hex));
    qDebug() << "Storing relay message for:" << nickname;
    return true;
}

void Bootstrap_Client::fetch_messages(const QString &nickname)
{
    connect_and_send(QString("FETCH:%1").arg(nickname));
    qDebug() << "Fetching relay messages for:" << nickname;
}

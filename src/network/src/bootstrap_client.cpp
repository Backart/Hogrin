#include "bootstrap_client.h"
#include "../../common/config.h"
#include <QDebug>

Bootstrap_Client::Bootstrap_Client(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, [this](){
        QString response = QString::fromUtf8(m_socket->readAll()).trimmed();
        qDebug() << "Bootstrap response:" << response;

        if (response.startsWith("FOUND:")) {
            QString data = response.mid(6);
            QStringList parts = data.split("|");
            if (parts.size() == 3) {
                QString    host    = parts[0];
                quint16    port    = parts[1].toUShort();
                QByteArray pub_key = QByteArray::fromHex(parts[2].toUtf8());
                emit user_found("", host, port, pub_key);
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
        else if (response.contains("MSG:")) {
            // Несколько сообщений разделены \n
            QList<QByteArray> result;
            for (const QString &line : response.split("\n")) {
                if (line.startsWith("MSG:")) {
                    QByteArray blob = QByteArray::fromHex(line.mid(4).toUtf8());
                    if (!blob.isEmpty())
                        result << blob;
                }
            }
            emit messages_fetched(result);
            qDebug() << "Fetched" << result.size() << "relay messages";
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
    connect_and_send(QString("FIND:%1").arg(nickname));
    qDebug() << "Finding user:" << nickname;
}

void Bootstrap_Client::unregister_user(const QString &nickname)
{
    connect_and_send(QString("UNREGISTER:%1").arg(nickname));
    qDebug() << "Unregistering from bootstrap:" << nickname;
}

void Bootstrap_Client::store_message(const QString &nickname,
                                     const QByteArray &encrypted_blob)
{
    QString hex = QString::fromUtf8(encrypted_blob.toHex());
    connect_and_send(QString("STORE:%1:%2").arg(nickname).arg(hex));
    qDebug() << "Storing relay message for:" << nickname;
}

void Bootstrap_Client::fetch_messages(const QString &nickname)
{
    connect_and_send(QString("FETCH:%1").arg(nickname));
    qDebug() << "Fetching relay messages for:" << nickname;
}

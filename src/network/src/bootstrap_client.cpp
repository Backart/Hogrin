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
            int sep = data.lastIndexOf("|");
            QString host = data.left(sep);
            quint16 port = data.mid(sep + 1).toUShort();
            //if (host != m_socket->localAddress().toString())
                emit user_found("", host, port);
        } else if (response == "NOT_FOUND") {
            emit user_not_found("");
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

void Bootstrap_Client::register_user(const QString &nickname, quint16 port)
{
    connect_and_send(QString("REGISTER:%1:%2").arg(nickname).arg(port));
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

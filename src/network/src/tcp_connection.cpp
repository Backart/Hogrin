#include "tcp_connection.h"
#include <QDataStream>

Tcp_Connection::Tcp_Connection(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_expectedSize(-1)
{
    m_socket->setParent(this);

    m_lastActivity = QDateTime::currentMSecsSinceEpoch();
    m_pingTimer = new QTimer(this);
    connect(m_pingTimer, &QTimer::timeout, this, &Tcp_Connection::checkPing);
    m_pingTimer->start(2000);

    connect(m_socket, &QTcpSocket::readyRead, this, &Tcp_Connection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &Tcp_Connection::onSocketDisconnected);
}

void Tcp_Connection::checkPing()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (now - m_lastActivity > 8000) {
        qWarning() << "P2P connection timed out (Ping failed). Closing zombie socket.";
        close();
        return;
    }

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray packet;
        QDataStream out(&packet, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << (qint32)0;
        m_socket->write(packet);
        m_socket->flush();
    }
}

void Tcp_Connection::sendMessage(const QByteArray &data)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);

    out << (qint32)data.size();
    out.writeRawData(data.constData(), data.size());

    m_socket->write(packet);
    m_socket->flush();
}

void Tcp_Connection::onReadyRead()
{
    m_lastActivity = QDateTime::currentMSecsSinceEpoch();
    m_buffer.append(m_socket->readAll());

    while (true) {
        if (m_expectedSize == -1) {
            if (m_buffer.size() < (qint64)sizeof(qint32)) break;

            QDataStream in(m_buffer);
            in.setVersion(QDataStream::Qt_6_0);
            in >> m_expectedSize;
            m_buffer.remove(0, sizeof(qint32));
        }

        if (m_expectedSize == 0) {
            m_expectedSize = -1;
            continue;
        }

        if (m_buffer.size() < m_expectedSize) break;

        QByteArray data = m_buffer.left(m_expectedSize);
        m_buffer.remove(0, m_expectedSize);
        m_expectedSize = -1;

        emit dataReceived(data);
    }
}

void Tcp_Connection::onSocketDisconnected()
{
    emit disconnected();
}

bool Tcp_Connection::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void Tcp_Connection::close()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->abort();
    }
}

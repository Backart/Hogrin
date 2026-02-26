#include "tcp_connection.h"
#include <QDataStream>

Tcp_Connection::Tcp_Connection(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &Tcp_Connection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &Tcp_Connection::onSocketDisconnected);
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
}

void Tcp_Connection::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    while (true) {
        if (m_expectedSize == -1) {
            if (m_buffer.size() < (qint64)sizeof(qint32)) break;

            QDataStream in(m_buffer);
            in.setVersion(QDataStream::Qt_6_0);
            in >> m_expectedSize;
            m_buffer.remove(0, sizeof(qint32));
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

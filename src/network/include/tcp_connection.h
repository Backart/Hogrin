#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>

class Tcp_Connection : public QObject
{
    Q_OBJECT
public:
    explicit Tcp_Connection(QTcpSocket *socket, QObject *parent = nullptr);

    void sendMessage(const QByteArray &data);
    bool isConnected() const;

signals:
    void dataReceived(const QByteArray &data);
    void disconnected();

private slots:
    void onReadyRead();
    void onSocketDisconnected();

private:
    QTcpSocket *m_socket;
    QByteArray m_buffer;
    qint32 m_expectedSize = -1;
};

#endif // TCP_CONNECTION_H

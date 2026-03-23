#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QTimer>
#include <QDateTime>

class Tcp_Connection : public QObject
{
    Q_OBJECT
public:
    explicit Tcp_Connection(QTcpSocket *socket, QObject *parent = nullptr);

    void sendMessage(const QByteArray &data);
    bool isConnected() const;

    void close();

signals:
    void dataReceived(const QByteArray &data);
    void disconnected();

private slots:
    void onReadyRead();
    void onSocketDisconnected();

    void checkPing();

private:
    QTcpSocket *m_socket;
    QByteArray m_buffer;
    qint32 m_expectedSize = -1;

    QTimer *m_pingTimer;
    qint64 m_lastActivity;
};

#endif // TCP_CONNECTION_H

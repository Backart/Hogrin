#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QString>
#include <QTcpServer>
#include <QList>

/**
 * @brief Класс для управления низкоуровневыми сетевыми соединениями.
 * Отвечает за транспортный уровень (TCP) в P2P-мессенджере.
 */
class Network_Manager : public QObject{
    Q_OBJECT // for signal and slots

public:
    explicit Network_Manager(QObject *parent = nullptr);

    // method for external control
    void connect_to_host(const QString &host, quint16 port);
    void disconnect_from_host();
    void send_data(const QByteArray &data);
    bool start_server(quint16 port);

signals:
    // signals to inform Messenger_Core
    void connected();
    void disconnected();
    void data_received(const QByteArray &data);
    void error_occurred(const QString &errorString);

private slots:
    // iternal socket event handling
    void on_ready_read();
    void on_connected();
    void on_disconnectig();
    void on_error_occurred(QAbstractSocket::SocketError socketError);

    void on_new_connection();
private:
    QTcpSocket *m_socket;
    QList<QTcpSocket*> m_sockets;
    QTcpServer *m_server;
};

#endif

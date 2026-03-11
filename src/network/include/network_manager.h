#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTimer>
#include <QList>
#include "tcp_connection.h"
#include "../common/config.h"


class Network_Manager : public QObject {
    Q_OBJECT

public:
    explicit Network_Manager(QObject *parent = nullptr);

    // Управление
    void connect_to_host(const QString &host,
                         quint16 port,
                         const QByteArray &my_pub_key);
    bool start_server(quint16 port);
    void disconnect_from_host();

    // Отправка
    void send_data(const QByteArray &data);

    bool has_connections() const;
    quint16 listening_port() const;
    void set_skip_p2p(bool skip) { m_skip_p2p = skip; }

    void set_nickname(const QString &nickname) { m_nickname = nickname; }

    QString m_nickname;

signals:
    // Сигналы для ядра
    void connected();
    void disconnected();
    void data_received(const QByteArray &data);

    void p2p_failed();
    void incoming_peer_authenticated(const QString &peer_nickname, const QByteArray &peer_pub_key);

private slots:
    // Внутренние слоты
    void handle_new_connection();
    void handle_data_received(const QByteArray &data);

private:
    QTcpServer *m_server;

    // Список активных соеденений
    QList<Tcp_Connection*> m_connections;

    bool m_handshake_done = false;
    void handle_handshake(Tcp_Connection *connection, const QByteArray &data);

    QTimer *m_p2p_timeout_timer;
    bool m_skip_p2p = false;
};

#endif

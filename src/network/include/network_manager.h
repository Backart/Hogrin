#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QList>
#include "tcp_connection.h"


class Network_Manager : public QObject {
    Q_OBJECT

public:
    explicit Network_Manager(QObject *parent = nullptr);

    // Управление
    void connect_to_host(const QString &host, quint16 port);
    bool start_server(quint16 port);
    void disconnect_from_host();

    // Отправка
    void send_data(const QByteArray &data);

signals:
    // Сигналы для ядра
    void connected();
    void disconnected();
    void data_received(const QByteArray &data);

private slots:
    // Внутренние слоты
    void handle_new_connection();
    void handle_data_received(const QByteArray &data);

private:
    QTcpServer *m_server;

    // Список активных соеденений
    QList<Tcp_Connection*> m_connections;
};

#endif

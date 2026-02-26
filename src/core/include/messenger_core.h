#ifndef MESSENGER_CORE_H
#define MESSENGER_CORE_H

#include <QObject>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QString>
#include <QByteArray>
#include <map>
#include <functional>

#include "../network/include/network_manager.h"
#include "../common/types.h"


class Messenger_Core: public QObject{
    Q_OBJECT // for signal and slots

public:

    explicit Messenger_Core(QObject *parent = nullptr);

    void send_message(const QString &text, const QString &username);
    bool start_server(quint16 port);
    void connect_to_host(const QString &host, quint16 port);

signals:

    void message_received(const QString &text, const QString &username);

private slots:

    void handle_data_received(const QByteArray &data);

private:
    std::map<MessageType, std::function<void(const DataPacket&)>> m_handlers;

    Network_Manager *m_network;

    DataPacket deserialize_packet(const QByteArray &bytes);
    QByteArray serialize_packet(const DataPacket &packet);
    void setup_handlers();

};


#endif // MESSENGER_CORE_H

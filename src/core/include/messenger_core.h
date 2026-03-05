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
#include "db_manager.h"
#include "bootstrap_client.h"
#include "crypto_manager.h"


class Messenger_Core: public QObject{
    Q_OBJECT // for signal and slots

public:

    explicit Messenger_Core(QObject *parent = nullptr);

    void send_message(const QString &text, const QString &username);
    bool start_server(quint16 port);
    void connect_to_host(const QString &host, quint16 port);

    bool connect_to_database(const QString &host, int port,
                           const QString &dbName,
                           const QString &user,
                           const QString &password);
    bool register_user(const QString &nickname, const QString &passwordHash);
    bool login_user(const QString &nickname, const QString &passwordHash);

    void create_session(const QString &nickname, const QString &token);
    bool session_exists(const QString &token);
    void remove_session(const QString &token);
    void update_last_seen(const QString &nickname);

    void register_on_bootstrap(const QString &nickname);
    void find_peer(const QString &nickname);

    void unregister_from_bootstrap(const QString &nickname);

signals:

    void message_received(const QString &text, const QString &username);
    void peer_found(const QString &nickname, const QString &host, quint16 port);
    void peer_not_found(const QString &nickname);

private slots:

    void handle_data_received(const QByteArray &data);
    void on_peer_found(const QString &nickname,
                       const QString &host,
                       quint16 port,
                       const QByteArray &peer_public_key);

private:

    std::map<MessageType, std::function<void(const DataPacket&)>> m_handlers;

    Network_Manager  *m_network;
    DB_Manager       *m_db;
    Bootstrap_Client *m_bootstrap;
    Crypto_Manager   *m_crypto;

    quint16 m_listening_port = 0;

    QByteArray serialize_packet(const DataPacket &packet);
    DataPacket deserialize_packet(const QByteArray &bytes);
    void       setup_handlers();


};


#endif // MESSENGER_CORE_H

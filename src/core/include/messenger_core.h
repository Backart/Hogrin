#ifndef MESSENGER_CORE_H
#define MESSENGER_CORE_H

#include <QObject>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QString>
#include <QByteArray>
#include <map>
#include <functional>
#include <QTimer>
#include <QDataStream>
#include <QDebug>
#include <QMap>
#include <QQueue>
#include <QNetworkInterface>

#include "../network/include/network_manager.h"
#include "../common/types.h"
#include "bootstrap_client.h"
#include "crypto_manager.h"
#include "local_db.h"

struct PeerState {
    bool relay_mode = false;
};

class Messenger_Core : public QObject {
    Q_OBJECT

public:
    explicit Messenger_Core(QObject *parent = nullptr);
    ~Messenger_Core();

    // Returns saved message DB id (or -1 on failure)
    int  send_message(const QString &peer,
                     const QString &text,
                     int reply_to_id                = -1,
                     const QString &reply_to_text   = "",
                     const QString &reply_to_sender = "");

    bool start_server(quint16 port);
    void connect_to_host(const QString &peer_nickname, const QString &host, quint16 port);

    void auth_register(const QString &nickname, const QString &password);
    void auth_login(const QString &nickname, const QString &password);
    void auth_verify(const QString &token);
    void auth_logout(const QString &token);

    void register_on_bootstrap(const QString &nickname);
    void find_peer(const QString &nickname);
    void unregister_from_bootstrap(const QString &nickname);

    void set_current_nickname(const QString &nickname);

    QList<Message_Record> load_history(const QString &peer, int limit = 100);
    QStringList get_recent_chats() const;

    quint16 get_listening_port() const;
    bool    bootstrap_is_ws()    const;

signals:
    void peer_found(const QString &nickname,
                    const QString &host,
                    quint16 port,
                    bool is_relay);
    void peer_not_found(const QString &nickname);
    void relay_mode_activated();

    void register_completed(bool success, const QString &error);
    void login_completed(bool success, const QString &error,
                         const QString &token, const QString &nickname);
    void verify_completed(bool success, const QString &nickname);

    // Incoming message — includes reply info and DB id
    void message_received(const QString &sender,
                          const QString &text,
                          int msg_id,
                          int reply_to_id,
                          const QString &reply_to_text,
                          const QString &reply_to_sender);

    // Outgoing message was saved — carry DB id for status tracking in QML
    void outgoing_message_sent(const QString &peer,
                               const QString &text,
                               const QDateTime &timestamp,
                               int msg_id);

    // Status update: MsgPending/MsgSent/MsgFailed/MsgRead
    void message_status_changed(int msg_id, int status);

    // Read receipt from peer: mark all our outgoing to peer up to this timestamp
    void messages_read_up_to(const QString &peer, qint64 timestamp);

    // Bootstrap transport
    void bootstrap_transport_changed(bool is_ipv4);

private slots:
    void handle_data_received(const QByteArray &data);
    void on_peer_found(const QString &nickname,
                       const QString &host,
                       quint16 port,
                       const QByteArray &peer_public_key);
    void on_p2p_failed(const QString &peer_nickname);
    void poll_relay_messages();
    void retry_p2p_connections();

private:
    std::map<MessageType, std::function<void(const DataPacket&, const QString&)>> m_handlers;

    Network_Manager  *m_network;
    Bootstrap_Client *m_bootstrap;

    quint16 m_listening_port = 0;

    QByteArray serialize_packet(const DataPacket &packet);
    DataPacket deserialize_packet(const QByteArray &bytes);
    void       setup_handlers();

    Crypto_Manager *crypto_for(const QString &peer);
    Crypto_Manager *m_identity_crypto = nullptr;

    QTimer  *m_poll_timer;
    QTimer  *m_p2p_retry_timer;
    QString  m_current_nickname;
    bool     m_relay_mode = false;

    Local_DB *m_local_db;

    QMap<QString, Crypto_Manager *> m_crypto_map;
    QMap<QString, PeerState>        m_peer_state;

    QList<QByteArray> m_undecryptable_messages;
    void try_decrypt_pending();

    void flush_pending_via_relay(const QString &peer, Crypto_Manager *crypto);

    QMap<QString, QByteArray> m_pending_verification;

    // Queue of message IDs waiting for relay store_confirmed/store_failed
    QQueue<int> m_pending_relay_ids;

    void send_read_receipt(const QString &peer, qint64 last_timestamp);
};

#endif // MESSENGER_CORE_H
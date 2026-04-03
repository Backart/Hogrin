#ifndef UI_HANDLER_H
#define UI_HANDLER_H

#include <QObject>
#include <QSettings>
#include <QUuid>

#include "../core/include/messenger_core.h"

class UI_Handler : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool bootstrapIsIPv4
                   READ bootstrapIsIPv4
                   NOTIFY bootstrap_transport_changed)

public:
    explicit UI_Handler(Messenger_Core *core, QObject *parent = nullptr);

    // Returns saved message DB id (for status tracking in QML), or -1
    Q_INVOKABLE int send_message_from_ui(const QString &peer,
                                         const QString &text,
                                         int reply_to_id                = -1,
                                         const QString &reply_to_text   = "",
                                         const QString &reply_to_sender = "");

    Q_INVOKABLE bool start_server(quint16 port);
    Q_INVOKABLE void connect_to_host(const QString &peer_nickname,
                                     const QString &host,
                                     quint16 port);

    Q_INVOKABLE void register_user(const QString &nickname, const QString &password);
    Q_INVOKABLE void login_user(const QString &nickname, const QString &password);

    Q_INVOKABLE void check_saved_session();
    Q_INVOKABLE void logout();

    Q_INVOKABLE void register_on_bootstrap(const QString &nickname);
    Q_INVOKABLE void find_peer(const QString &nickname);

    void unregister_from_bootstrap(const QString &nickname);

    Q_INVOKABLE QVariantList load_history(const QString &peer, int limit = 100);
    Q_INVOKABLE QStringList  get_recent_chats();

    quint16 get_listening_port() const;
    bool    bootstrapIsIPv4()    const { return m_bootstrap_is_ipv4; }

signals:
    // Incoming message — includes reply context and DB id
    void message_received(QString username, QString text, QDateTime time,
                          int msg_id, int reply_to_id,
                          QString reply_to_text, QString reply_to_sender);

    void session_restored(QString nickname);

    void peer_status_changed(QString nickname, bool isOnline, bool isRelay);
    void peer_found(QString host, quint16 port);
    void peer_not_found();
    void relay_mode_activated();

    void login_result(bool success, const QString &error);
    void register_result(bool success, const QString &error);

    // Message delivery status update
    void message_status_changed(int msg_id, int status);

    // Peer has read our messages up to this timestamp
    void messages_read_up_to(QString peer, qint64 timestamp);

    // Bootstrap transport changed (false = IPv6 TCP, true = IPv4 WS)
    void bootstrap_transport_changed(bool is_ipv4);

private:
    Messenger_Core *m_core;
    QString         m_current_nickname_cache;
    bool            m_bootstrap_is_ipv4 = false;
};

#endif // UI_HANDLER_H
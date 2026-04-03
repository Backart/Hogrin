#include "ui_handler.h"

UI_Handler::UI_Handler(Messenger_Core *core, QObject *parent)
    : QObject(parent)
    , m_core(core)
{
    // ── Incoming message ──────────────────────────────────────────────────────
    connect(m_core, &Messenger_Core::message_received,
            this, [this](const QString &sender, const QString &text,
                   int msg_id, int reply_to_id,
                   const QString &reply_to_text,
                   const QString &reply_to_sender) {
                emit message_received(sender, text,
                                      QDateTime::currentDateTime(),
                                      msg_id, reply_to_id,
                                      reply_to_text, reply_to_sender);
            });

    // ── Peer status ───────────────────────────────────────────────────────────
    connect(m_core, &Messenger_Core::peer_found,
            this, [this](const QString &nickname, const QString &host,
                   quint16 port, bool is_relay) {
                emit peer_status_changed(nickname, true, is_relay);
                emit peer_found(host, port);
            });

    connect(m_core, &Messenger_Core::peer_not_found,
            this, [this](const QString &nickname) {
                emit peer_status_changed(nickname, false, false);
                emit peer_not_found();
            });

    connect(m_core, &Messenger_Core::relay_mode_activated,
            this, &UI_Handler::relay_mode_activated);

    // ── Auth ──────────────────────────────────────────────────────────────────
    connect(m_core, &Messenger_Core::register_completed,
            this, [this](bool success, const QString &error) {
                emit register_result(success, error);
            });

    connect(m_core, &Messenger_Core::login_completed,
            this, [this](bool success, const QString &error,
                   const QString &token, const QString &nickname) {
                if (success) {
                    m_current_nickname_cache = nickname;
                    QSettings settings("Hogrin", "Hogrin");
                    settings.setValue("session/token",    token);
                    settings.setValue("session/nickname", nickname);
                }
                emit login_result(success, error);
            });

    connect(m_core, &Messenger_Core::verify_completed,
            this, [this](bool success, const QString &nickname) {
                if (success) {
                    m_current_nickname_cache = nickname;
                    emit session_restored(nickname);
                } else {
                    QSettings settings("Hogrin", "Hogrin");
                    settings.remove("session/token");
                    settings.remove("session/nickname");
                    emit session_restored("");
                }
            });

    // ── Message status ────────────────────────────────────────────────────────
    connect(m_core, &Messenger_Core::message_status_changed,
            this, &UI_Handler::message_status_changed);

    connect(m_core, &Messenger_Core::messages_read_up_to,
            this, &UI_Handler::messages_read_up_to);

    // ── Bootstrap transport ───────────────────────────────────────────────────
    connect(m_core, &Messenger_Core::bootstrap_transport_changed,
            this, [this](bool is_ipv4) {
                m_bootstrap_is_ipv4 = is_ipv4;
                emit bootstrap_transport_changed(is_ipv4);
            });
}

// ── public API ────────────────────────────────────────────────────────────────

int UI_Handler::send_message_from_ui(const QString &peer, const QString &text,
                                     int reply_to_id, const QString &reply_to_text, const QString &reply_to_sender)
{
    return m_core->send_message(peer, text, reply_to_id, reply_to_text, reply_to_sender);
}

bool UI_Handler::start_server(quint16 port)
{ return m_core->start_server(port); }

void UI_Handler::connect_to_host(const QString &peer_nickname,
                                 const QString &host, quint16 port)
{ m_core->connect_to_host(peer_nickname, host, port); }

void UI_Handler::register_user(const QString &nickname, const QString &password)
{ m_core->auth_register(nickname, password); }

void UI_Handler::login_user(const QString &nickname, const QString &password)
{ m_core->auth_login(nickname, password); }

void UI_Handler::check_saved_session()
{
    QSettings settings("Hogrin", "Hogrin");
    QString token    = settings.value("session/token").toString();
    QString nickname = settings.value("session/nickname").toString();

    if (token.isEmpty() || nickname.isEmpty()) {
        emit session_restored("");
        return;
    }
    m_core->auth_verify(token);
}

void UI_Handler::logout()
{
    QSettings settings("Hogrin", "Hogrin");
    QString token    = settings.value("session/token").toString();
    QString nickname = settings.value("session/nickname").toString();

    if (!nickname.isEmpty()) unregister_from_bootstrap(nickname);
    if (!token.isEmpty())    m_core->auth_logout(token);

    settings.remove("session/token");
    settings.remove("session/nickname");
}

void UI_Handler::unregister_from_bootstrap(const QString &nickname)
{ m_core->unregister_from_bootstrap(nickname); }

void UI_Handler::register_on_bootstrap(const QString &nickname)
{ m_core->register_on_bootstrap(nickname); }

void UI_Handler::find_peer(const QString &nickname)
{ m_core->find_peer(nickname); }

QVariantList UI_Handler::load_history(const QString &peer, int limit)
{
    QVariantList result;
    for (const Message_Record &msg : m_core->load_history(peer, limit)) {
        QVariantMap map;
        map["id"]            = msg.id;
        map["peer"]          = msg.peer;
        map["sender"]        = msg.sender;
        map["text"]          = msg.text;
        map["timestamp"]     = msg.timestamp.toSecsSinceEpoch();
        map["is_outgoing"]   = msg.is_outgoing;
        map["is_delivered"]  = msg.is_delivered;
        map["status"]        = msg.status;
        map["reply_to_id"]   = msg.reply_to_id;
        map["reply_to_text"] = msg.reply_to_text;
        map["reply_to_sender"]  = msg.reply_to_sender;
        map["is_read"]       = msg.is_read;
        result.append(map);
    }
    return result;
}

QStringList UI_Handler::get_recent_chats()
{ return m_core->get_recent_chats(); }

quint16 UI_Handler::get_listening_port() const
{ return m_core->get_listening_port(); }
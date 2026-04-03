#include "messenger_core.h"

Messenger_Core::Messenger_Core(QObject *parent)
    : QObject(parent)
    , m_network(new Network_Manager(this))
    , m_bootstrap(new Bootstrap_Client(this))
    , m_poll_timer(new QTimer(this))
    , m_local_db(new Local_DB(this))
    , m_identity_crypto(new Crypto_Manager())
{
    setup_handlers();

    // ── Bootstrap transport ───────────────────────────────────────────────────
    connect(m_bootstrap, &Bootstrap_Client::transport_changed,
            this, [this](bool is_ws) {
                emit bootstrap_transport_changed(is_ws);
            });

    // ── Network restored ──────────────────────────────────────────────────────
    connect(m_network, &Network_Manager::network_restored,
            this, [this]() {
                if (!m_current_nickname.isEmpty()) {
                    m_bootstrap->ensure_connected();
                    m_bootstrap->fetch_messages(m_current_nickname);
                }
            });

    connect(m_network, &Network_Manager::data_received,
            this, &Messenger_Core::handle_data_received);

    connect(m_network, &Network_Manager::p2p_failed,
            this, &Messenger_Core::on_p2p_failed);

    // ── P2P retry ─────────────────────────────────────────────────────────────
    m_p2p_retry_timer = new QTimer(this);
    connect(m_p2p_retry_timer, &QTimer::timeout,
            this, &Messenger_Core::retry_p2p_connections);
    m_p2p_retry_timer->start(5 * 60 * 1000);

    // ── Incoming peer authenticated ───────────────────────────────────────────
    connect(m_network, &Network_Manager::incoming_peer_authenticated,
            this, [this](const QString &peer_nickname, const QByteArray &peer_pub_key) {
                Crypto_Manager *peer_crypto = crypto_for(peer_nickname);
                peer_crypto->set_identity(*m_identity_crypto);
                if (!peer_crypto->compute_shared_secret(peer_pub_key)) {
                    qWarning() << "Failed to compute shared secret for:" << peer_nickname;
                    return;
                }
                qDebug() << "Shared secret computed for incoming P2P from:" << peer_nickname;
                m_peer_state[peer_nickname].relay_mode = false;
                m_relay_mode = false;
                emit peer_found(peer_nickname, "", 0, false);
                try_decrypt_pending();

                if (m_local_db->has_pending(peer_nickname)) {
                    const QList<QByteArray> pending = m_local_db->peek_outbox(peer_nickname);
                    for (const QByteArray &blob : pending)
                        m_network->send_data(peer_crypto->encrypt(blob));
                    m_local_db->confirm_outbox(peer_nickname);
                }
            });

    connect(m_network, &Network_Manager::incoming_peer_needs_verification,
            this, [this](const QString &nickname, const QByteArray &claimed_pubkey) {
                m_pending_verification[nickname] = claimed_pubkey;
                m_bootstrap->find_user(nickname);
            });

    // ── Bootstrap signals ─────────────────────────────────────────────────────
    connect(m_bootstrap, &Bootstrap_Client::user_found,
            this, &Messenger_Core::on_peer_found);
    connect(m_bootstrap, &Bootstrap_Client::user_not_found,
            this, &Messenger_Core::peer_not_found);

    // ── Relay store status ────────────────────────────────────────────────────
    connect(m_bootstrap, &Bootstrap_Client::store_confirmed,
            this, [this]() {
                if (!m_pending_relay_ids.isEmpty()) {
                    int id = m_pending_relay_ids.dequeue();
                    m_local_db->update_status(id, MsgSent);
                    emit message_status_changed(id, MsgSent);
                }
            });

    connect(m_bootstrap, &Bootstrap_Client::store_failed,
            this, [this](const QString &) {
                if (!m_pending_relay_ids.isEmpty()) {
                    int id = m_pending_relay_ids.dequeue();
                    m_local_db->update_status(id, MsgFailed);
                    emit message_status_changed(id, MsgFailed);
                }
            });

    // ── Relay messages fetched ────────────────────────────────────────────────
    connect(m_bootstrap, &Bootstrap_Client::messages_fetched,
            this, [this](const QList<QByteArray> &messages) {
                for (const QByteArray &blob : messages) {
                    int first  = blob.indexOf('|');
                    int second = blob.indexOf('|', first + 1);
                    if (first == -1 || second == -1) {
                        handle_data_received(blob);
                        continue;
                    }

                    QString    sender_nick = QString::fromUtf8(blob.left(first));
                    QByteArray peer_pubkey = QByteArray::fromHex(blob.mid(first + 1, second - first - 1));
                    QByteArray encrypted   = blob.mid(second + 1);

                    Crypto_Manager *crypto = crypto_for(sender_nick);
                    if (!crypto->is_ready()) {
                        crypto->set_identity(*m_identity_crypto);
                        crypto->compute_shared_secret(peer_pubkey);
                    }

                    QByteArray decrypted = crypto->decrypt(encrypted);
                    if (!decrypted.isEmpty()) {
                        if (!m_peer_state[sender_nick].relay_mode) {
                            m_peer_state[sender_nick].relay_mode = true;
                            m_relay_mode = true;
                            m_network->disconnect_from_host();
                            emit relay_mode_activated();
                        }

                        DataPacket packet = deserialize_packet(decrypted);
                        auto it = m_handlers.find(packet.type);
                        if (it != m_handlers.end())
                            it->second(packet, sender_nick);
                    } else {
                        qWarning() << "Undecryptable relay message from:" << sender_nick;
                    }
                }
            });

    // ── Poll timer ────────────────────────────────────────────────────────────
    connect(m_poll_timer, &QTimer::timeout,
            this, &Messenger_Core::poll_relay_messages);

    // ── Reconnect ─────────────────────────────────────────────────────────────
    connect(m_bootstrap, &Bootstrap_Client::reconnected,
            this, [this]() {
                m_network->set_skip_p2p(false);
                if (!m_current_nickname.isEmpty())
                    register_on_bootstrap(m_current_nickname);
            });

    // ── All P2P connections lost → relay ──────────────────────────────────────
    connect(m_network, &Network_Manager::all_connections_lost,
            this, [this]() {
                for (auto &state : m_peer_state)
                    state.relay_mode = true;
                m_relay_mode = true;
                emit relay_mode_activated();

                for (auto it = m_peer_state.begin(); it != m_peer_state.end(); ++it) {
                    const QString &peer = it.key();
                    if (!m_local_db->has_pending(peer)) continue;
                    Crypto_Manager *crypto = crypto_for(peer);
                    if (!crypto->is_ready()) continue;

                    const QList<QByteArray> pending = m_local_db->peek_outbox(peer);
                    for (const QByteArray &blob : pending) {
                        QByteArray encrypted = crypto->encrypt(blob);
                        QString    pubkey_hex = QString::fromUtf8(m_identity_crypto->public_key().toHex());
                        QByteArray header     = (m_current_nickname + "|" + pubkey_hex + "|").toUtf8();
                        m_bootstrap->store_message(peer, header + encrypted);
                    }
                    m_local_db->confirm_outbox(peer);
                }
            });

    // ── Auth ──────────────────────────────────────────────────────────────────
    connect(m_bootstrap, &Bootstrap_Client::auth_register_success,
            this, [this]() { emit register_completed(true, ""); });

    connect(m_bootstrap, &Bootstrap_Client::auth_register_failed,
            this, [this](const QString &reason) { emit register_completed(false, reason); });

    connect(m_bootstrap, &Bootstrap_Client::auth_login_success,
            this, [this](const QString &token, const QString &nickname) {
                set_current_nickname(nickname);
                m_poll_timer->start(Config::RELAY_POLL_INTERVAL_MS);
                emit login_completed(true, "", token, nickname);
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_login_failed,
            this, [this](const QString &reason) {
                emit login_completed(false, reason, "", "");
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_verify_success,
            this, [this](const QString &nickname) {
                set_current_nickname(nickname);
                m_poll_timer->start(Config::RELAY_POLL_INTERVAL_MS);
                emit verify_completed(true, nickname);
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_verify_failed,
            this, [this](const QString &) { emit verify_completed(false, ""); });

    connect(m_bootstrap, &Bootstrap_Client::registered,
            this, [this](const QString &server_seen_ip) {
                const auto addresses = QNetworkInterface::allAddresses();
                bool local_match = false;
                for (const QHostAddress &addr : addresses)
                    if (addr.toString() == server_seen_ip) { local_match = true; break; }

                if (!local_match) {
                    qDebug() << "NAT/CGNAT detected:" << server_seen_ip << "— relay-first";
                    m_network->set_skip_p2p(true);
                }
            });
}

Messenger_Core::~Messenger_Core()
{
    m_poll_timer->stop();
    m_p2p_retry_timer->stop();
    qDeleteAll(m_crypto_map);
    delete m_identity_crypto;
    delete m_local_db;
    m_local_db = nullptr;
}

Crypto_Manager *Messenger_Core::crypto_for(const QString &peer)
{
    auto it = m_crypto_map.find(peer);
    if (it != m_crypto_map.end()) return it.value();
    Crypto_Manager *crypto = new Crypto_Manager();
    m_crypto_map.insert(peer, crypto);
    return crypto;
}

bool Messenger_Core::bootstrap_is_ws() const
{
    return m_bootstrap->is_ws();
}

// ── serialization ────────────────────────────────────────────────────────────

QByteArray Messenger_Core::serialize_packet(const DataPacket &packet)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<quint8>(packet.type)
           << packet.timestamp
           << packet.data;
    return bytes;
}

DataPacket Messenger_Core::deserialize_packet(const QByteArray &bytes)
{
    DataPacket packet;
    quint8 type_value = 0;
    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_6_0);
    stream >> type_value >> packet.timestamp >> packet.data;

    if (stream.status() != QDataStream::Ok)
        qDebug() << "deserialize_packet: stream error";
    else
        packet.type = static_cast<MessageType>(type_value);

    return packet;
}

// ── incoming data ────────────────────────────────────────────────────────────

void Messenger_Core::handle_data_received(const QByteArray &data)
{
    if (data.isEmpty()) return;

    int sep = data.indexOf('|');
    if (sep <= 0) {
        m_undecryptable_messages.append(data);
        return;
    }

    QString    sender    = QString::fromUtf8(data.left(sep));
    QByteArray encrypted = data.mid(sep + 1);

    Crypto_Manager *crypto = crypto_for(sender);
    if (!crypto->is_ready()) {
        m_undecryptable_messages.append(data);
        return;
    }

    QByteArray decrypted = crypto->decrypt(encrypted);
    if (decrypted.isEmpty()) {
        qWarning() << "Decryption failed from:" << sender;
        return;
    }

    DataPacket packet = deserialize_packet(decrypted);
    auto it = m_handlers.find(packet.type);
    if (it != m_handlers.end())
        it->second(packet, sender);
}

// ── handlers ─────────────────────────────────────────────────────────────────

void Messenger_Core::setup_handlers()
{
    // ── ChatMessage ───────────────────────────────────────────────────────────
    m_handlers[MessageType::ChatMessage] = [this](const DataPacket &packet,
                                                  const QString &sender_hint) {
        QDataStream stream(packet.data);
        stream.setVersion(QDataStream::Qt_6_0);

        QString sender_name, message_text, reply_to_text, reply_to_sender;
        qint32  reply_to_id = -1;
        stream >> sender_name >> message_text;
        if (!stream.atEnd())
            stream >> reply_to_id >> reply_to_text >> reply_to_sender;

        if (stream.status() != QDataStream::Ok || sender_name.isEmpty())
            sender_name = sender_hint;

        int msg_id = m_local_db->save_message(
            sender_name, sender_name, message_text,
            packet.timestamp, false,
            reply_to_id, reply_to_text, reply_to_sender);

        send_read_receipt(sender_name, packet.timestamp.toSecsSinceEpoch());

        emit message_received(sender_name, message_text,
                              msg_id, reply_to_id,
                              reply_to_text, reply_to_sender);
    };

    // ── ReadReceipt ───────────────────────────────────────────────────────────
    m_handlers[MessageType::ReadReceipt] = [this](const DataPacket &packet,
                                                  const QString &sender_hint) {
        QDataStream stream(packet.data);
        stream.setVersion(QDataStream::Qt_6_0);

        QString sender_name;
        qint64  last_timestamp = 0;
        stream >> sender_name >> last_timestamp;
        if (sender_name.isEmpty()) sender_name = sender_hint;

        m_local_db->mark_read_up_to(sender_name, last_timestamp);
        emit messages_read_up_to(sender_name, last_timestamp);
    };
}

void Messenger_Core::send_read_receipt(const QString &peer, qint64 last_timestamp)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << m_current_nickname << last_timestamp;

    DataPacket packet;
    packet.type      = MessageType::ReadReceipt;
    packet.timestamp = QDateTime::currentDateTimeUtc();
    packet.data      = payload;

    QByteArray bytes = serialize_packet(packet);

    Crypto_Manager *crypto = crypto_for(peer);
    if (!crypto->is_ready()) return;

    QByteArray encrypted = crypto->encrypt(bytes);

    if (m_peer_state[peer].relay_mode) {
        QString    pubkey_hex = QString::fromUtf8(m_identity_crypto->public_key().toHex());
        QByteArray header     = (m_current_nickname + "|" + pubkey_hex + "|").toUtf8();
        m_bootstrap->store_message(peer, header + encrypted);
    } else if (m_network->has_connections()) {
        QByteArray blob = (m_current_nickname + "|").toUtf8() + encrypted;
        m_network->send_data(blob);
    }
    // If no connection, skip — receipts are best-effort
}

// ── send ─────────────────────────────────────────────────────────────────────

int Messenger_Core::send_message(const QString &peer, const QString &text,
                                 int reply_to_id, const QString &reply_to_text,
                                 const QString &reply_to_sender)
{
    if (text.isEmpty() || peer.isEmpty() || peer == m_current_nickname) return -1;

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << m_current_nickname << text
           << (qint32)reply_to_id << reply_to_text << reply_to_sender;

    DataPacket packet;
    packet.type      = MessageType::ChatMessage;
    packet.timestamp = QDateTime::currentDateTimeUtc();
    packet.data      = payload;

    QByteArray bytes = serialize_packet(packet);

    int msg_id = m_local_db->save_message(peer, m_current_nickname, text,
                                          packet.timestamp, true,
                                          reply_to_id, reply_to_text,
                                          reply_to_sender);

    Crypto_Manager *crypto = crypto_for(peer);

    if (!crypto->is_ready()) {
        m_local_db->enqueue_message(peer, bytes);
        m_local_db->update_status(msg_id, MsgPending);
    } else {
        QByteArray encrypted = crypto->encrypt(bytes);

        if (m_peer_state[peer].relay_mode) {
            QString    pubkey_hex = QString::fromUtf8(m_identity_crypto->public_key().toHex());
            QByteArray header     = (m_current_nickname + "|" + pubkey_hex + "|").toUtf8();
            m_bootstrap->store_message(peer, header + encrypted);
            m_pending_relay_ids.enqueue(msg_id);
            m_local_db->update_status(msg_id, MsgPending);
        } else if (m_network->has_connections()) {
            QByteArray blob = (m_current_nickname + "|").toUtf8() + encrypted;
            m_network->send_data(blob);
            m_local_db->update_status(msg_id, MsgSent);
        } else {
            m_local_db->enqueue_message(peer, bytes);
            m_local_db->update_status(msg_id, MsgPending);
        }
    }

    emit outgoing_message_sent(peer, text, packet.timestamp.toLocalTime(), msg_id);
    return msg_id;
}
// ── network ──────────────────────────────────────────────────────────────────

bool Messenger_Core::start_server(quint16 port)
{
    if (m_listening_port != 0) return true;
    bool ok = m_network->start_server(port);
    if (ok) m_listening_port = m_network->listening_port();
    return ok;
}

void Messenger_Core::connect_to_host(const QString &peer_nickname,
                                     const QString &host, quint16 port)
{
    m_network->connect_to_host(peer_nickname, host, port, m_identity_crypto->public_key());
}

// ── auth ─────────────────────────────────────────────────────────────────────

void Messenger_Core::auth_register(const QString &nickname, const QString &password)
{ m_bootstrap->auth_register(nickname, password); }

void Messenger_Core::auth_login(const QString &nickname, const QString &password)
{ m_bootstrap->auth_login(nickname, password); }

void Messenger_Core::auth_verify(const QString &token)
{ m_bootstrap->auth_verify(token); }

void Messenger_Core::auth_logout(const QString &token)
{ m_bootstrap->auth_logout(token); }

// ── bootstrap ────────────────────────────────────────────────────────────────

void Messenger_Core::register_on_bootstrap(const QString &nickname)
{
    if (m_listening_port == 0) { qWarning() << "Cannot register: server not started"; return; }
    m_bootstrap->register_user(nickname, m_listening_port, m_identity_crypto->public_key());
}

void Messenger_Core::find_peer(const QString &nickname)
{ m_bootstrap->find_user(nickname); }

void Messenger_Core::unregister_from_bootstrap(const QString &nickname)
{ m_bootstrap->unregister_user(nickname); }

// ── peer found ───────────────────────────────────────────────────────────────

void Messenger_Core::on_peer_found(const QString &nickname,
                                   const QString &host,
                                   quint16 port,
                                   const QByteArray &peer_public_key)
{
    if (m_pending_verification.contains(nickname)) {
        QByteArray claimed = m_pending_verification.take(nickname);
        if (claimed != peer_public_key) {
            qWarning() << "SECURITY: MITM for:" << nickname;
            m_network->reject_incoming_peer(nickname);
            return;
        }
        m_network->confirm_incoming_peer(nickname, peer_public_key);
        return;
    }

    m_local_db->save_contact_key(nickname, peer_public_key);

    Crypto_Manager *crypto = crypto_for(nickname);
    if (!crypto->is_ready()) {
        crypto->set_identity(*m_identity_crypto);
        if (!crypto->compute_shared_secret(peer_public_key)) {
            qWarning() << "Failed to compute shared secret for:" << nickname;
            return;
        }
        try_decrypt_pending();
    }

    if (m_peer_state[nickname].relay_mode) {
        flush_pending_via_relay(nickname, crypto);
        emit peer_found(nickname, host, port, true);
        return;
    }

    if (!m_network->has_connections())
        m_network->connect_to_host(nickname, host, port, m_identity_crypto->public_key());

    emit peer_found(nickname, host, port, false);
}

void Messenger_Core::on_p2p_failed(const QString &peer_nickname)
{
    qDebug() << "P2P failed for" << peer_nickname << "— relay";
    m_peer_state[peer_nickname].relay_mode = true;
    m_relay_mode = true;
    m_poll_timer->start(Config::RELAY_POLL_INTERVAL_MS);
    emit relay_mode_activated();
    flush_pending_via_relay(peer_nickname, crypto_for(peer_nickname));
}

// ── poll / misc ───────────────────────────────────────────────────────────────

void Messenger_Core::poll_relay_messages()
{
    if (m_current_nickname.isEmpty()) return;
    m_bootstrap->ensure_connected();
    m_bootstrap->fetch_messages(m_current_nickname);
}

void Messenger_Core::set_current_nickname(const QString &nickname)
{
    m_current_nickname = nickname;
    m_network->set_nickname(nickname);

    if (m_local_db) delete m_local_db;
    m_local_db = new Local_DB();

    if (!m_local_db->init(nickname)) {
        qWarning() << "Local_DB init failed for:" << nickname;
        return;
    }

    QByteArray saved_pub, saved_sec;
    if (m_local_db->load_identity(saved_pub, saved_sec)) {
        m_identity_crypto->load_keypair(saved_pub, saved_sec);
        qDebug() << "Identity keys loaded.";
    } else {
        m_local_db->save_identity(m_identity_crypto->public_key(),
                                  m_identity_crypto->secret_key());
        qDebug() << "New identity keys generated and saved.";
    }
}

QList<Message_Record> Messenger_Core::load_history(const QString &peer, int limit)
{ return m_local_db->load_history(peer, limit); }

QStringList Messenger_Core::get_recent_chats() const
{ return m_local_db->get_recent_chats(); }

quint16 Messenger_Core::get_listening_port() const
{ return m_listening_port; }

void Messenger_Core::try_decrypt_pending()
{
    if (m_undecryptable_messages.isEmpty()) return;
    QList<QByteArray> remaining;

    for (const QByteArray &data : m_undecryptable_messages) {
        int sep = data.indexOf('|');
        if (sep <= 0) { remaining.append(data); continue; }

        QString    sender    = QString::fromUtf8(data.left(sep));
        QByteArray encrypted = data.mid(sep + 1);

        Crypto_Manager *crypto = crypto_for(sender);
        if (!crypto->is_ready()) { remaining.append(data); continue; }

        QByteArray decrypted = crypto->decrypt(encrypted);
        if (decrypted.isEmpty()) { remaining.append(data); continue; }

        DataPacket packet = deserialize_packet(decrypted);
        auto it = m_handlers.find(packet.type);
        if (it != m_handlers.end()) it->second(packet, sender);
    }
    m_undecryptable_messages = remaining;
}

void Messenger_Core::retry_p2p_connections()
{
    if (m_current_nickname.isEmpty()) return;
    for (auto it = m_peer_state.begin(); it != m_peer_state.end(); ++it) {
        if (it.value().relay_mode) {
            it.value().relay_mode = false;
            m_relay_mode = false;
            m_bootstrap->find_user(it.key());
        }
    }
}

void Messenger_Core::flush_pending_via_relay(const QString &peer, Crypto_Manager *crypto)
{
    if (!m_local_db->has_pending(peer)) return;
    const QList<QByteArray> pending = m_local_db->peek_outbox(peer);
    for (const QByteArray &blob : pending) {
        if (!crypto->is_ready()) break;
        QByteArray encrypted  = crypto->encrypt(blob);
        QString    pubkey_hex = QString::fromUtf8(m_identity_crypto->public_key().toHex());
        QByteArray header     = (m_current_nickname + "|" + pubkey_hex + "|").toUtf8();
        m_bootstrap->store_message(peer, header + encrypted);
    }
    m_local_db->confirm_outbox(peer);
}
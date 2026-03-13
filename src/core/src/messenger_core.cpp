#include "messenger_core.h"

Messenger_Core::Messenger_Core(QObject *parent)
    : QObject(parent)
    , m_network(new Network_Manager(this))
    , m_bootstrap(new Bootstrap_Client(this))
    , m_poll_timer(new QTimer(this))
    , m_local_db(new Local_DB(this))
{
    setup_handlers();

    connect(m_network, &Network_Manager::data_received,
            this, &Messenger_Core::handle_data_received);

    connect(m_network, &Network_Manager::p2p_failed,
            this, &Messenger_Core::on_p2p_failed);

    connect(m_network, &Network_Manager::incoming_peer_authenticated,
            this, [this](const QString &peer_nickname, const QByteArray &peer_pub_key) {
                Crypto_Manager *peer_crypto = crypto_for(peer_nickname);
                peer_crypto->set_identity(*m_identity_crypto);
                if (!peer_crypto->compute_shared_secret(peer_pub_key)) {
                    qWarning() << "Failed to compute shared secret for:" << peer_nickname;
                } else {
                    qDebug() << "Shared secret computed for incoming P2P from:" << peer_nickname;

                    m_peer_state[peer_nickname].relay_mode = false;
                    try_decrypt_pending();

                    if (m_local_db->has_pending(peer_nickname)) {
                        const QList<QByteArray> pending = m_local_db->peek_outbox(peer_nickname);
                        for (const QByteArray &blob : pending) {
                            QByteArray encrypted = peer_crypto->encrypt(blob);
                            m_network->send_data(encrypted);
                        }
                        m_local_db->confirm_outbox(peer_nickname);
                        qDebug() << "Flushed pending outbox to incoming peer:" << peer_nickname;
                    }
                }
            });

    connect(m_bootstrap, &Bootstrap_Client::user_found,
            this, &Messenger_Core::on_peer_found);
    connect(m_bootstrap, &Bootstrap_Client::user_not_found,
            this, &Messenger_Core::peer_not_found);

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
                    handle_data_received(encrypted);
                }
            });

    connect(m_poll_timer, &QTimer::timeout,
            this, &Messenger_Core::poll_relay_messages);

    // ── async auth ───────────────────────────────────────────────────────────

    connect(m_bootstrap, &Bootstrap_Client::auth_register_success,
            this, [this]() {
                emit register_completed(true, "");
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_register_failed,
            this, [this](const QString &reason) {
                emit register_completed(false, reason);
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_login_success,
            this, [this](const QString &token, const QString &nickname) {
                set_current_nickname(nickname);
                m_poll_timer->start(1000);
                emit login_completed(true, "", token, nickname);
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_login_failed,
            this, [this](const QString &reason) {
                emit login_completed(false, reason, "", "");
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_verify_success,
            this, [this](const QString &nickname) {
                set_current_nickname(nickname);
                m_poll_timer->start(1000);
                emit verify_completed(true, nickname);
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_verify_failed,
            this, [this](const QString &reason) {
                Q_UNUSED(reason)
                emit verify_completed(false, "");
            });

    connect(m_bootstrap, &Bootstrap_Client::registered,
            this, [this](const QString &server_seen_ip) {
                const auto addresses = QNetworkInterface::allAddresses();
                bool local_match = false;
                for (const QHostAddress &addr : addresses) {
                    if (addr.toString() == server_seen_ip) {
                        local_match = true;
                        break;
                    }
                }
                if (!local_match) {
                    qDebug() << "NAT/CGNAT detected: server sees"
                             << server_seen_ip << "— enabling relay-first mode";
                    m_network->set_skip_p2p(true);
                }
            });
}

Messenger_Core::~Messenger_Core()
{
    qDeleteAll(m_crypto_map);
    delete m_identity_crypto;
}

Crypto_Manager *Messenger_Core::crypto_for(const QString &peer)
{
    auto it = m_crypto_map.find(peer);
    if (it != m_crypto_map.end())
        return it.value();

    Crypto_Manager *crypto = new Crypto_Manager();
    m_crypto_map.insert(peer, crypto);
    return crypto;
}

// ── serialization ────────────────────────────────────────────────────────────

QByteArray Messenger_Core::serialize_packet(const DataPacket &packet)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<quint8>(packet.type);
    stream << packet.timestamp;
    stream << packet.data;
    return bytes;
}

DataPacket Messenger_Core::deserialize_packet(const QByteArray &bytes)
{
    DataPacket packet;
    quint8 type_value = 0;
    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_6_0);
    stream >> type_value;
    stream >> packet.timestamp;
    stream >> packet.data;

    if (stream.status() != QDataStream::Ok) {
        qDebug() << "Error: Failed to deserialize packet!";
    } else {
        packet.type = static_cast<MessageType>(type_value);
    }
    return packet;
}

// ── incoming data ────────────────────────────────────────────────────────────

void Messenger_Core::handle_data_received(const QByteArray &data)
{
    if (data.isEmpty()) return;

    QByteArray decrypted;

    for (auto it = m_crypto_map.begin(); it != m_crypto_map.end(); ++it) {
        if (it.value()->is_ready()) {
            decrypted = it.value()->decrypt(data);
            if (!decrypted.isEmpty()) break;
        }
    }

    if (decrypted.isEmpty()) {
        qWarning() << "Decryption failed (no matching key yet) — saving to undecryptable queue";
        m_undecryptable_messages.append(data);
        return;
    }

    DataPacket packet = deserialize_packet(decrypted);
    auto it = m_handlers.find(packet.type);
    if (it != m_handlers.end()) {
        it->second(packet);
    } else {
        qDebug() << "Warning: No handler for packet type:" << static_cast<quint8>(packet.type);
    }
}

void Messenger_Core::setup_handlers()
{
    m_handlers[MessageType::ChatMessage] = [this](const DataPacket &packet) {
        QDataStream stream(packet.data);
        QString sender_name;
        QString message_text;
        stream >> sender_name >> message_text;

        if (stream.status() != QDataStream::Ok) {
            qDebug() << "Error reading chat message stream";
            return;
        }

        m_local_db->save_message(sender_name,
                                 sender_name,
                                 message_text,
                                 packet.timestamp,
                                 false);

        qDebug() << "New message from" << sender_name
                 << ":" << message_text
                 << packet.timestamp.toString("hh:mm:ss");

        emit message_received(sender_name, message_text);
    };
}

// ── send ─────────────────────────────────────────────────────────────────────

void Messenger_Core::send_message(const QString &username, const QString &text)
{
    if (text.isEmpty()) return;

    if (m_pending_peer.isEmpty()) {
        qWarning() << "Message dropped: No pending peer set!";
        return;
    }

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream << username << text;

    DataPacket packet;
    packet.type      = MessageType::ChatMessage;
    packet.timestamp = QDateTime::currentDateTime();
    packet.data      = payload;

    QByteArray bytes = serialize_packet(packet);

    Crypto_Manager *crypto = crypto_for(m_pending_peer);

    if (!crypto->is_ready()) {
        qWarning() << "Key exchange not done — queuing message for:" << m_pending_peer;
        m_local_db->enqueue_message(m_pending_peer, bytes);
    } else {
        QByteArray encrypted = crypto->encrypt(bytes);

        if (m_peer_state[m_pending_peer].relay_mode) {
            QString pubkey_hex = QString::fromUtf8(m_identity_crypto->public_key().toHex());
            QByteArray header  = (m_current_nickname + "|" + pubkey_hex + "|").toUtf8();
            m_bootstrap->store_message(m_pending_peer, header + encrypted);
            qDebug() << "Message sent via relay to:" << m_pending_peer;
        } else if (m_network->has_connections()) {
            m_network->send_data(encrypted);
        } else {
            qDebug() << "No connection — queuing message for:" << m_pending_peer;
            m_local_db->enqueue_message(m_pending_peer, bytes); // В очередь всегда ложим до шифрования
        }
    }

    m_local_db->save_message(m_pending_peer,
                             username,
                             text,
                             packet.timestamp,
                             true);

    emit message_received(username, text);
}

// ── network ──────────────────────────────────────────────────────────────────

bool Messenger_Core::start_server(quint16 port)
{
    if (m_listening_port != 0) return true;
    bool ok = m_network->start_server(port);
    if (ok) m_listening_port = m_network->listening_port();
    return ok;
}

void Messenger_Core::connect_to_host(const QString &host, quint16 port)
{
    Crypto_Manager *crypto = crypto_for(m_pending_peer);
    m_network->connect_to_host(host, port, m_identity_crypto->public_key());
}

// ── auth ─────────────────────────────────────────────────────────────────────

void Messenger_Core::auth_register(const QString &nickname, const QString &password)
{
    m_bootstrap->auth_register(nickname, password);
}

void Messenger_Core::auth_login(const QString &nickname, const QString &password)
{
    m_bootstrap->auth_login(nickname, password);
}

void Messenger_Core::auth_verify(const QString &token)
{
    m_bootstrap->auth_verify(token);
}

void Messenger_Core::auth_logout(const QString &token)
{
    m_bootstrap->auth_logout(token);
}

// ── bootstrap ────────────────────────────────────────────────────────────────

void Messenger_Core::register_on_bootstrap(const QString &nickname)
{
    if (m_listening_port == 0) {
        qWarning() << "Cannot register: server not started yet";
        return;
    }
    m_bootstrap->register_user(nickname, m_listening_port, m_identity_crypto->public_key());
}

void Messenger_Core::find_peer(const QString &nickname)
{
    m_bootstrap->find_user(nickname);
}

void Messenger_Core::unregister_from_bootstrap(const QString &nickname)
{
    m_bootstrap->unregister_user(nickname);
}

// ── peer found ───────────────────────────────────────────────────────────────

void Messenger_Core::on_peer_found(const QString &nickname,
                                   const QString &host,
                                   quint16 port,
                                   const QByteArray &peer_public_key)
{
    m_local_db->save_contact_key(nickname, peer_public_key);
    m_pending_peer = nickname;
    m_peer_state[nickname].relay_mode = false;
    m_relay_mode = false;

    Crypto_Manager *crypto = crypto_for(nickname);

    crypto->set_identity(*m_identity_crypto);
    if (!crypto->compute_shared_secret(peer_public_key)) {
        qWarning() << "Failed to compute shared secret for:" << nickname;
    } else {
        qDebug() << "Shared secret computed immediately for:" << nickname;
        try_decrypt_pending();
    }

    if (m_local_db->has_pending(nickname)) {
        const QList<QByteArray> pending = m_local_db->peek_outbox(nickname);
        for (const QByteArray &blob : pending) {
            if (crypto->is_ready()) {
                QByteArray encrypted   = crypto->encrypt(blob);
                QString    pubkey_hex  = QString::fromUtf8(m_identity_crypto->public_key().toHex());
                QByteArray header      = (m_current_nickname + "|" + pubkey_hex + "|").toUtf8();
                m_bootstrap->store_message(nickname, header + encrypted);
            } else {
                qWarning() << "Cannot send pending message: crypto is not ready!";
            }
        }

        connect(m_bootstrap, &Bootstrap_Client::store_confirmed,
                this, [this, nickname]() {
                    m_local_db->confirm_outbox(nickname);
                }, Qt::SingleShotConnection);

        connect(m_bootstrap, &Bootstrap_Client::store_failed,
                this, [](const QString &reason) {
                    qWarning() << "Relay store failed, keeping outbox:" << reason;
                }, Qt::SingleShotConnection);
    }

    m_network->connect_to_host(host, port, m_identity_crypto->public_key());
    emit peer_found(nickname, host, port);
}

void Messenger_Core::on_p2p_failed()
{
    qDebug() << "P2P failed for" << m_pending_peer << "— switching to relay";
    m_peer_state[m_pending_peer].relay_mode = true;
    m_relay_mode = true; // кэш

    m_poll_timer->start(Config::RELAY_POLL_INTERVAL_MS);
    emit relay_mode_activated();
}

// ── relay poll ───────────────────────────────────────────────────────────────

void Messenger_Core::poll_relay_messages()
{
    if (m_current_nickname.isEmpty()) return;
    m_bootstrap->fetch_messages(m_current_nickname);
}

// ── misc ─────────────────────────────────────────────────────────────────────

void Messenger_Core::set_current_nickname(const QString &nickname)
{
    m_current_nickname = nickname;
    m_network->set_nickname(nickname);

    if (!m_identity_crypto)
        m_identity_crypto = new Crypto_Manager();

    if (!m_local_db->init(nickname)) {
        qWarning() << "Local_DB init failed for:" << nickname;
        return;
    }

    QByteArray saved_pub, saved_sec;
    if (m_local_db->load_identity(saved_pub, saved_sec)) {
        m_identity_crypto->load_keypair(saved_pub, saved_sec);
        qDebug() << "Identity keys successfully loaded from database.";
    } else {
        m_local_db->save_identity(m_identity_crypto->public_key(),
                                  m_identity_crypto->secret_key());
        qDebug() << "Generated and saved NEW identity keys to database.";
    }
}

QList<Message_Record> Messenger_Core::load_history(const QString &peer, int limit)
{
    return m_local_db->load_history(peer, limit);
}

QStringList Messenger_Core::get_recent_chats() const
{
    return m_local_db->get_recent_chats();
}

quint16 Messenger_Core::get_listening_port() const
{
    return m_listening_port;
}

void Messenger_Core::try_decrypt_pending()
{
    if (m_undecryptable_messages.isEmpty()) return;

    QList<QByteArray> remaining;
    for (const QByteArray &data : m_undecryptable_messages) {
        QByteArray decrypted;

        for (auto it = m_crypto_map.begin(); it != m_crypto_map.end(); ++it) {
            if (it.value()->is_ready()) {
                decrypted = it.value()->decrypt(data);
                if (!decrypted.isEmpty()) break;
            }
        }
        if (!decrypted.isEmpty()) {
            DataPacket packet = deserialize_packet(decrypted);
            auto handler_it = m_handlers.find(packet.type);
            if (handler_it != m_handlers.end()) handler_it->second(packet);
        } else {
            remaining.append(data);
        }
    }
    m_undecryptable_messages = remaining;
}

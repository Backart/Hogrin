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
            this, [this](const QByteArray &peer_pub_key) {

            Crypto_Manager *crypto = crypto_for(m_pending_peer);
            crypto->set_server_mode(true);

            if (!crypto->compute_shared_secret(peer_pub_key)) {
                qWarning() << "Failed to compute shared secret for:" << m_pending_peer;
            } else {
                qDebug() << "Shared secret computed for incoming P2P from:" << m_pending_peer;
            }
    });

    connect(m_bootstrap, &Bootstrap_Client::user_found,
            this, &Messenger_Core::on_peer_found);
    connect(m_bootstrap, &Bootstrap_Client::user_not_found,
            this, &Messenger_Core::peer_not_found);

    connect(m_bootstrap, &Bootstrap_Client::messages_fetched,
            this, [this](const QList<QByteArray> &messages) {
                for (const QByteArray &blob : messages) {
                    handle_data_received(blob);
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
                emit login_completed(true, "", token, nickname);
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_login_failed,
            this, [this](const QString &reason) {
                emit login_completed(false, reason, "", "");
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_verify_success,
            this, [this](const QString &nickname) {
                set_current_nickname(nickname);
                emit verify_completed(true, nickname);
            });

    connect(m_bootstrap, &Bootstrap_Client::auth_verify_failed,
            this, [this](const QString &reason) {
                Q_UNUSED(reason)
                emit verify_completed(false, "");
            });
}

Messenger_Core::~Messenger_Core()
{
    qDeleteAll(m_crypto_map);
}

Crypto_Manager *Messenger_Core::crypto_for(const QString &peer)
{
    auto it = m_crypto_map.find(peer);
    if (it != m_crypto_map.end())
        return it.value();

    Crypto_Manager *crypto = new Crypto_Manager(); // не QObject — без parent
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

    QByteArray decrypted = data;

    Crypto_Manager *crypto = crypto_for(m_pending_peer);

    if (crypto->is_ready()) {
        decrypted = crypto->decrypt(data);
        if (decrypted.isEmpty()) {
            qWarning() << "Decryption failed — dropping packet";
            return;
        }
    }

    DataPacket packet = deserialize_packet(decrypted);

    auto it = m_handlers.find(packet.type);
    if (it != m_handlers.end()) {
        it->second(packet);
    } else {
        qDebug() << "Warning: No handler for packet type:"
                 << static_cast<quint8>(packet.type);
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
                                 false); // входящее

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

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream << username << text;

    DataPacket packet;
    packet.type      = MessageType::ChatMessage;
    packet.timestamp = QDateTime::currentDateTime();
    packet.data      = payload;

    QByteArray bytes = serialize_packet(packet);

    Crypto_Manager *crypto = crypto_for(m_pending_peer);
    if (crypto->is_ready()) {
        bytes = crypto->encrypt(bytes);
    } else {
        qWarning() << "Sending unencrypted — key exchange not done";
    }

    if (m_relay_mode) {
        if (m_pending_peer.isEmpty()) {
            qWarning() << "Relay mode but no pending peer set — message dropped";
            return;
        }
        m_bootstrap->store_message(m_pending_peer, bytes);
        qDebug() << "Message sent via relay to:" << m_pending_peer;
    } else if (m_network->has_connections()) {
        m_network->send_data(bytes);
    } else {
        qDebug() << "No connection — queuing message for:" << m_pending_peer;
        m_local_db->enqueue_message(m_pending_peer, bytes);
    }

    m_local_db->save_message(m_pending_peer,
                             username,
                             text,
                             packet.timestamp,
                             true); // исходящее

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
    m_network->connect_to_host(host, port, crypto->public_key());
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
    Crypto_Manager *crypto = crypto_for(nickname);
    m_bootstrap->register_user(nickname, m_listening_port, crypto->public_key());
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
    m_pending_peer = nickname;

    m_peer_state[nickname].relay_mode = false;
    m_relay_mode = false;

    Crypto_Manager *crypto = crypto_for(nickname);
    crypto->set_server_mode(false);

    if (!crypto->compute_shared_secret(peer_public_key)) {
        qWarning() << "Key exchange failed with peer:" << nickname;
        return;
    }

    if (m_local_db->has_pending(nickname)) {
        qDebug() << "Retransmitting queued messages to:" << nickname;
        for (const QByteArray &blob : m_local_db->dequeue_messages(nickname)) {
            m_bootstrap->store_message(nickname, blob);
        }
    }

    m_network->connect_to_host(host, port, crypto->public_key());

    qDebug() << "Key exchange OK, connecting to" << host << port;
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
    if (!m_local_db->init(nickname))
        qWarning() << "Local_DB init failed for:" << nickname;
}

QList<Message_Record> Messenger_Core::load_history(const QString &peer, int limit)
{
    return m_local_db->load_history(peer, limit);
}

quint16 Messenger_Core::get_listening_port() const
{
    return m_listening_port;
}

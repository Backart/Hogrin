#include "messenger_core.h"
#include <QDataStream>
#include <QDebug>

Messenger_Core::Messenger_Core(QObject *parent)
    : QObject(parent)
    , m_network(new Network_Manager(this))
    , m_db(new DB_Manager(this))
    , m_bootstrap(new Bootstrap_Client(this))
    , m_crypto(new Crypto_Manager())
    , m_poll_timer(new QTimer(this))
    , m_local_db(new Local_DB(this))
{
    setup_handlers();
    // connect(от_кого, &От_Кого::сигнал, кому, &Кому::слот);

    connect(m_network, &Network_Manager::data_received,
            this, &Messenger_Core::handle_data_received);

    connect(m_network, &Network_Manager::p2p_failed,
            this, &Messenger_Core::on_p2p_failed);

    connect(m_bootstrap, &Bootstrap_Client::user_found,
            this, &Messenger_Core::on_peer_found);
    connect(m_bootstrap, &Bootstrap_Client::user_not_found,
            this, &Messenger_Core::peer_not_found);

    // Relay
    connect(m_bootstrap, &Bootstrap_Client::messages_fetched,
            this, [this](const QList<QByteArray> &messages){
                for (const QByteArray &blob : messages) {
                    handle_data_received(blob);
                }
            });

    connect(m_poll_timer, &QTimer::timeout,
            this, &Messenger_Core::poll_relay_messages);
}

QByteArray Messenger_Core::serialize_packet(const DataPacket &packet){
    QByteArray bytes;

    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint8>(packet.type);
    stream << packet.timestamp;
    stream << packet.data;

    return bytes;
}

DataPacket Messenger_Core::deserialize_packet(const QByteArray &bytes){
    DataPacket packet;

    quint8 type_value;

    QDataStream stream(bytes); //QIODevice::ReadOnly defolt
    stream.setVersion(QDataStream::Qt_6_0);

    stream >> type_value;
    packet.type = static_cast<MessageType>(type_value);
    stream >> packet.timestamp;
    stream >> packet.data;

    if (stream.status() != QDataStream::Ok) {
        qDebug() << "Error: Failed to deserialize packet!";
    } else {
        packet.type = static_cast<MessageType>(type_value);
    }

    return packet;
}

void Messenger_Core::handle_data_received(const QByteArray &data)
{
    if (data.isEmpty()) return;

    QByteArray decrypted = data;

    if (m_crypto->is_ready()) {
        decrypted = m_crypto->decrypt(data);
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

void Messenger_Core::setup_handlers(){
    m_handlers[MessageType::ChatMessage] = [this](const DataPacket &packet){
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
    // add new type
}

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

    if (m_crypto->is_ready()) {
        bytes = m_crypto->encrypt(bytes);
    } else {
        qWarning() << "Sending unencrypted — key exchange not done";
    }

    if (m_relay_mode) {
        if (m_pending_peer.isEmpty()) {
            qWarning() << "Relay mode but no pending peer set";
            m_local_db->enqueue_message(m_pending_peer, bytes); // в очередь
        } else {
            m_bootstrap->store_message(m_pending_peer, bytes);
            qDebug() << "Message sent via relay to:" << m_pending_peer;
        }
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
                             true);

    emit message_received(username, text);
}

bool Messenger_Core::start_server(quint16 port){
    m_listening_port = port;
    return m_network->start_server(port);
}

void Messenger_Core::connect_to_host(const QString &host, quint16 port){
    m_network->connect_to_host(host, port);
}

bool Messenger_Core::connect_to_database(const QString &host, int port,
                                       const QString &dbName,
                                       const QString &user,
                                       const QString &password)
{
    return m_db->connect(host, port, dbName, user, password);
}

bool Messenger_Core::register_user(const QString &nickname, const QString &passwordHash)
{
    return m_db->registerUser(nickname, passwordHash);
}

bool Messenger_Core::login_user(const QString &nickname, const QString &passwordHash)
{
    if (!m_db->validateUser(nickname, passwordHash))
        return false;
    m_db->updateLastSeen(nickname);
    return true;
}

void Messenger_Core::create_session(const QString &nickname, const QString &token)
{
    m_db->createSession(nickname, "0.0.0.0", token);
}

bool Messenger_Core::session_exists(const QString &token)
{
    return m_db->sessionExists(token);
}

void Messenger_Core::remove_session(const QString &token)
{
    m_db->removeSession(token);
}

void Messenger_Core::update_last_seen(const QString &nickname)
{
    m_db->updateLastSeen(nickname);
}

void Messenger_Core::register_on_bootstrap(const QString &nickname)
{
    if (m_listening_port == 0) {
        qWarning() << "Cannot register: server not started yet";
        return;
    }
    m_bootstrap->register_user(nickname,
                               m_listening_port,
                               m_crypto->public_key());
}

void Messenger_Core::find_peer(const QString &nickname)
{
    m_bootstrap->find_user(nickname);
}

void Messenger_Core::unregister_from_bootstrap(const QString &nickname)
{
    m_bootstrap->unregister_user(nickname);
}

void Messenger_Core::on_peer_found(const QString &nickname,
                                   const QString &host,
                                   quint16 port,
                                   const QByteArray &peer_public_key)
{
    m_pending_peer = nickname;
    m_relay_mode   = false;

    if (!m_crypto->compute_shared_secret(peer_public_key)) {
        qWarning() << "Key exchange failed with peer:" << nickname;
        return;
    }

    if (m_local_db->has_pending(nickname)) {
        qDebug() << "Retransmitting queued messages to:" << nickname;
        for (const QByteArray &blob : m_local_db->dequeue_messages(nickname)) {
            m_bootstrap->store_message(nickname, blob);
        }
    }

    qDebug() << "Key exchange OK, connecting to" << host << port;
    emit peer_found(nickname, host, port);
}

void Messenger_Core::on_p2p_failed()
{
    qDebug() << "P2P failed — switching to relay mode";
    m_relay_mode = true;

    m_poll_timer->start(Config::RELAY_POLL_INTERVAL_MS);

    emit relay_mode_activated();
}

void Messenger_Core::poll_relay_messages()
{
    if (m_current_nickname.isEmpty()) return;
    m_bootstrap->fetch_messages(m_current_nickname);
}

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

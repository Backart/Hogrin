#include "network_manager.h"

Network_Manager::Network_Manager(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    // Server connection
    connect(m_server, &QTcpServer::newConnection, this, &Network_Manager::handle_new_connection);

    qDebug() << "Network_Manager initialized. (constructor)";
}

bool Network_Manager::start_server(quint16 port){
    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "Server started on port" << port;
        return true;
    }
    qDebug() << "Server failed to start:" << m_server->errorString();
    return false;
}

void Network_Manager::connect_to_host(const QString &host,
                                      quint16 port,
                                      const QByteArray &my_pub_key)
{
    if (m_skip_p2p) {
        qDebug() << "NAT/CGNAT detected — skipping P2P attempt, going relay";
        emit p2p_failed();
        return;
    }
    // ─────────────────────────────────────────────────────────────────────────

    disconnect_from_host();
    qDebug() << "Connecting to" << host << ":" << port;

    QTcpSocket    *socket     = new QTcpSocket();
    Tcp_Connection *connection = new Tcp_Connection(socket, this);

    QTimer *timeout = new QTimer(this);
    timeout->setSingleShot(true);

    connect(socket, &QTcpSocket::connected,
            this, [this, connection, timeout, my_pub_key]() {
                timeout->stop();
                timeout->deleteLater();

                QByteArray payload = QByteArray(Config::HANDSHAKE_TOKEN)
                                     + ":"
                                     + m_nickname.toUtf8() + ":"
                                     + my_pub_key.toHex();
                connection->sendMessage(payload);

                emit connected();
            });

    connect(timeout, &QTimer::timeout,
            this, [this, socket, connection, timeout]() {
                qDebug() << "P2P connection timeout — fallback to relay";
                timeout->deleteLater();

                m_connections.removeAll(connection);
                connection->deleteLater();
                socket->abort();

                emit p2p_failed();
            });

    connect(connection, &Tcp_Connection::dataReceived,
            this, &Network_Manager::handle_data_received);

    connect(connection, &Tcp_Connection::disconnected,
            this, [this, connection]() {
                m_connections.removeAll(connection);
                connection->deleteLater();
                emit disconnected();
            });

    m_connections.append(connection);
    socket->connectToHost(host, port);
    timeout->start(Config::P2P_TIMEOUT_MS);
}

void Network_Manager::handle_new_connection(){
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        Tcp_Connection *connection = new Tcp_Connection(socket, this);

        connect(connection, &Tcp_Connection::dataReceived, this, [this, connection](const QByteArray &data){
            if (!connection->property("authenticated").toBool()) {
                QByteArray expected_token = QByteArray(Config::HANDSHAKE_TOKEN);

                if (data.startsWith(expected_token + ":")) {
                    QByteArray rest = data.mid(expected_token.size() + 1).trimmed();
                    int sep = rest.indexOf(':');
                    QString peer_nickname = QString::fromUtf8(rest.left(sep));
                    QByteArray peer_pub_key = QByteArray::fromHex(rest.mid(sep + 1));

                    connection->setProperty("authenticated", true);
                    qDebug() << "Client authenticated successfully. Peer key received.";

                    emit incoming_peer_authenticated(peer_nickname, peer_pub_key);
                    emit connected();
                } else {
                    qDebug() << "Invalid handshake — dropping connection";
                    connection->deleteLater();
                    m_connections.removeAll(connection);
                }
            } else {
                handle_data_received(data);
            }
        });

        connect(connection, &Tcp_Connection::disconnected, this, [this, connection](){
            m_connections.removeAll(connection);
            connection->deleteLater();
        });

        m_connections.append(connection);
        qDebug() << "New incoming connection! Waiting for handshake. Total peers:" << m_connections.size();
    }
}

void Network_Manager::send_data(const QByteArray &data){
    for(auto *conn : m_connections){
        conn->sendMessage(data);
    }
}

void Network_Manager::handle_data_received(const QByteArray &data){
    qDebug() << "handle_data_received called, size:" << data.size();
    emit data_received(data);
}

void Network_Manager::disconnect_from_host(){
    for (auto *conn : m_connections) {
        conn->close();
        conn->deleteLater();
    }
    m_connections.clear();
    emit disconnected();
}

bool Network_Manager::has_connections() const { return !m_connections.isEmpty(); }

quint16 Network_Manager::listening_port() const { return m_server->serverPort(); }

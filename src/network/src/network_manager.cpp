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

    if (!m_connections.isEmpty()) {
        qDebug() << "Already have connections — skipping new P2P attempt";
        return;
    }

    qDebug() << "Connecting to" << host << ":" << port;

    QTcpSocket     *socket     = new QTcpSocket();
    Tcp_Connection *connection = new Tcp_Connection(socket, this);

    QPointer<QTcpSocket> safe_socket = socket;
    QPointer<Tcp_Connection> safe_connection = connection;

    QTimer *timeout = new QTimer(this);
    timeout->setSingleShot(true);

    connect(socket, &QTcpSocket::connected,
            this, [this, connection, timeout, my_pub_key]() {
                timeout->stop();
                timeout->deleteLater();

                QByteArray payload = m_nickname.toUtf8() + ":" + my_pub_key.toHex();
                connection->sendMessage(payload);

                emit connected();
            });

    connect(timeout, &QTimer::timeout,
            this, [this, safe_socket, safe_connection, timeout]() {
                qDebug() << "P2P connection timeout — fallback to relay";

                if (timeout) {
                    timeout->deleteLater();
                }

                if (safe_connection) {
                    m_connections.removeAll(safe_connection);
                    safe_connection->deleteLater();
                }

                if (safe_socket) {
                    safe_socket->abort();
                }

                emit p2p_failed();
            });

    connect(connection, &Tcp_Connection::dataReceived,
            this, &Network_Manager::handle_data_received);

    connect(connection, &Tcp_Connection::disconnected,
            this, [this, connection]() {
                m_connections.removeAll(connection);
                connection->deleteLater();
                if (m_connections.isEmpty())
                    emit all_connections_lost();
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
                QByteArray payload = data.trimmed();
                int sep = payload.indexOf(':');
                if (sep > 0) {
                    QString peer_nickname = QString::fromUtf8(payload.left(sep));
                    QByteArray peer_pub_key = QByteArray::fromHex(payload.mid(sep + 1));

                    if (!peer_nickname.isEmpty() &&
                        peer_pub_key.size() == crypto_kx_PUBLICKEYBYTES) {

                        connection->setProperty("authenticated", true);
                        qDebug() << "Client authenticated successfully. Peer key received.";
                        emit incoming_peer_authenticated(peer_nickname, peer_pub_key);
                        emit connected();
                        return;
                    }
                }
                qDebug() << "Invalid handshake — dropping connection";
                connection->deleteLater();
                m_connections.removeAll(connection);
            } else {
                handle_data_received(data);
            }
        });

        connect(connection, &Tcp_Connection::disconnected, this, [this, connection](){
            m_connections.removeAll(connection);
            connection->deleteLater();
            if (m_connections.isEmpty())
                emit all_connections_lost();
            emit disconnected();
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

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

void Network_Manager::connect_to_host(const QString &host, quint16 port){
    qDebug() << "Connecting to" << host << ":" << port;
    QTcpSocket *socket = new QTcpSocket();

    Tcp_Connection *connection = new Tcp_Connection(socket, this);

    connect(socket, &QTcpSocket::connected, this, [connection](){
        connection->sendMessage(QByteArray(Config::HANDSHAKE_TOKEN));
    });

    socket->connectToHost(host, port);
    connect(connection, &Tcp_Connection::dataReceived, this, &Network_Manager::handle_data_received);
    connect(connection, &Tcp_Connection::disconnected, this, [this, connection](){
        m_connections.removeAll(connection);
        connection->deleteLater();
        emit disconnected();
    });
    m_connections.append(connection);
}

void Network_Manager::handle_new_connection(){
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        Tcp_Connection *connection = new Tcp_Connection(socket, this);

        // Ждём handshake токен — первые данные должны быть токеном
        connect(connection, &Tcp_Connection::dataReceived, this, [this, connection](const QByteArray &data){
            if (!connection->property("authenticated").toBool()) {
                qDebug() << "Handshake received, size:" << data.size() << "data:" << data.trimmed();
                qDebug() << "Expected token:" << QByteArray(Config::HANDSHAKE_TOKEN);
                if (data.trimmed() == QByteArray(Config::HANDSHAKE_TOKEN)) {
                    connection->setProperty("authenticated", true);
                    qDebug() << "Client authenticated successfully";
                    emit connected();
                } else {
                    qDebug() << "Invalid handshake token — dropping connection";
                    connection->deleteLater();
                    m_connections.removeAll(connection);
                    return;
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

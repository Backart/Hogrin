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
    socket->connectToHost(host, port);
    Tcp_Connection *connection = new Tcp_Connection(socket, this);

    connect(connection, &Tcp_Connection::dataReceived, this, &Network_Manager::handle_data_received);

    connect(connection, &Tcp_Connection::disconnected, this, [this, connection](){
        m_connections.removeAll(connection);
        connection->deleteLater();
        emit disconnected();
        qDebug() << "Connection lost. Remaining peers:" << m_connections.size();
    });


    m_connections.append(connection);
}

void Network_Manager::handle_new_connection(){

    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();


        Tcp_Connection *connection = new Tcp_Connection(socket, this);

        connect(connection, &Tcp_Connection::dataReceived, this, &Network_Manager::handle_data_received);

        connect(connection, &Tcp_Connection::disconnected, this, [this, connection](){
            m_connections.removeAll(connection);
            connection->deleteLater();
        });

        m_connections.append(connection);
        emit connected();
        qDebug() << "New incoming connection! Total peers:" << m_connections.size();
    }
}

void Network_Manager::send_data(const QByteArray &data){
    for(auto *conn : m_connections){
        conn->sendMessage(data);
    }
}

void Network_Manager::handle_data_received(const QByteArray &data){
    emit data_received(data);
}

void Network_Manager::disconnect_from_host(){
    for(auto *conn : m_connections){
        conn->deleteLater();
    }
    m_connections.clear();
    emit disconnected();
}

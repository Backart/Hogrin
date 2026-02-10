#include "network_manager.h"
#include <QDebug>


Network_Manager::Network_Manager(QObject *parent)
    : QObject(parent) // Передаем родителя в базовый класс Qt
    , m_socket(new QTcpSocket(this)) // Создаем сокет и привязываем его память к этому классу
    , m_server(new QTcpServer(this))
{
    // Здесь "прошиваем" логику (Сигналы и Слоты)
    // connect(от_кого, &От_Кого::сигнал, кому, &Кому::слот);

    connect(m_socket, &QTcpSocket::connected, this, &Network_Manager::on_connected);
    connect(m_socket, &QTcpSocket::readyRead, this, &Network_Manager::on_ready_read);

    connect(m_socket, &QTcpSocket::errorOccurred, this, &Network_Manager::on_error_occurred);

    connect(m_server, &QTcpServer::newConnection, this, &Network_Manager::on_new_connection);

    qDebug() << "Network_Manager initialized. (constructor)";
}

void Network_Manager::connect_to_host(const QString &host, quint16 port){
    m_socket->connectToHost(host, port);
}

void Network_Manager::disconnect_from_host(){

}

void Network_Manager::send_data(const QByteArray &data){

    if(m_socket->state() == QAbstractSocket::ConnectedState){
        m_socket->write(data);
    }

}

void Network_Manager::on_ready_read(){

    QTcpSocket *sendingSocket = qobject_cast<QTcpSocket*>(sender());

    if(!sendingSocket) return;

    QByteArray result = sendingSocket->readAll();

    emit data_received(result);
    qDebug() << "(on ready read) data received" << sendingSocket->peerAddress().toString() << ":" << result;
}

void Network_Manager::on_connected(){
    emit connected();

    qDebug() << "(on connected) Connected to host!";
}

void Network_Manager::on_disconnectig(){
    emit disconnected();

    qDebug() << "Disconnected to host!";
}

void Network_Manager::on_error_occurred(QAbstractSocket::SocketError socketError){
    emit error_occurred(m_socket->errorString());

    qDebug() << "Socket error:" << m_socket->errorString();
}

bool Network_Manager::start_server(quint16 port){
    bool success = m_server->listen(QHostAddress::Any, port);

    if(success)
        qDebug() << "(func start_server) Server started on port" << port;
    else
        qDebug() << "Server failed to start:" << m_server->errorString();

    return success;
}

void Network_Manager::on_new_connection(){

    QTcpSocket *clientSocket = m_server->nextPendingConnection(); // забираем сокет

    if(!clientSocket) return;

    m_sockets.append(clientSocket); // добавление в список

    connect(clientSocket, &QTcpSocket::readyRead, this, &Network_Manager::on_ready_read);

    connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket](){
        m_sockets.removeOne(clientSocket);
        clientSocket->deleteLater(); // clean memory
        qDebug() << "Client disconenected";
    });

    qDebug() << "(on new connection) New peer connected from:" << clientSocket->peerAddress().toString();
}


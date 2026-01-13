#include "network_manager.h"
#include <QDebug>

Network_Manager::Network_Manager(QObject *parent)
    : QObject(parent) // Передаем родителя в базовый класс Qt
    , m_socket(new QTcpSocket(this)) // Создаем сокет и привязываем его память к этому классу
{
    // Здесь "прошиваем" логику (Сигналы и Слоты)
    // connect(от_кого, &От_Кого::сигнал, кому, &Кому::слот);

    connect(m_socket, &QTcpSocket::connected, this, &Network_Manager::on_connectig);
    connect(m_socket, &QTcpSocket::readyRead, this, &Network_Manager::on_ready_read);

    connect(m_socket, &QTcpSocket::errorOccurred, this, &Network_Manager::on_error_occurred);

    qDebug() << "Network_Manager: инициализирован и готов к работе.";
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
    QByteArray result = m_socket->readAll();

    emit data_received(result);

    qDebug() << "data received" << result;

}

void Network_Manager::on_connectig(){
    emit connected();

    qDebug() << "Connected to host!";
}

void Network_Manager::on_disconnectig(){
    emit disconnected();

    qDebug() << "Disconnected to host!";
}

void Network_Manager::on_error_occurred(QAbstractSocket::SocketError socketError){
    emit error_occurred(m_socket->errorString());

    qDebug() << "Socket error:" << m_socket->errorString();
}





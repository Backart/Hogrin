#include "messenger_core.h"
#include <QDataStream>
#include <QDebug>

Messenger_Core::Messenger_Core(QObject *parent)
    : QObject(parent)
    , m_network(new Network_Manager(this))
{
    setup_handlers();
    // connect(от_кого, &От_Кого::сигнал, кому, &Кому::слот);

    connect(m_network, &Network_Manager::data_received, this, &Messenger_Core::handle_data_received);
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

void Messenger_Core::handle_data_received(const QByteArray &data){

    if(data.isEmpty()) return;

    DataPacket packet = deserialize_packet(data);

    auto iterator = m_handlers.find(packet.type);

    if(iterator != m_handlers.end()){
        iterator->second(packet);
    }else{
        qDebug() << "Warning: No handler registered for packet type:"
                 << static_cast<quint8>(packet.type);
    }
}

void Messenger_Core::setup_handlers(){
    m_handlers[MessageType::ChatMessage] = [this](const DataPacket &packet){

        QDataStream stream(packet.data);

        QString senderName;
        QString messageText;

        stream >> senderName >> messageText;

        if(stream.status() != QDataStream::Ok){
            qDebug() << "Error reading chat message stream";
            return;
        }

        qDebug() << "New message from" << senderName
                 << ":"<< messageText
                 << packet.timestamp.toString("hh:mm:ss");


        emit message_received(senderName, messageText);
    };
    // add new type
}

void Messenger_Core::send_message(const QString &username, const QString &text){

    if(text.isEmpty()) return;

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);

    stream << username << text;

    DataPacket packet;
    packet.type = MessageType::ChatMessage;
    packet.timestamp = QDateTime::currentDateTime();
    packet.data = payload;

    QByteArray bytes = serialize_packet(packet);

    if(m_network)
        m_network->send_data(bytes);

    emit message_received(username, text);
}

bool Messenger_Core::start_server(quint16 port){
    return m_network->start_server(port);
}

void Messenger_Core::connect_to_host(const QString &host, quint16 port){
    m_network->connect_to_host(host, port);
}



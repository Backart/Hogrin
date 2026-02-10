#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QByteArray>
#include <QDateTime>

/**
 * @brief Типы сообщений нашего протокола.
 * Каждое сообщение в сети будет начинаться с этого числа.
 */
enum class MessageType : quint8{
    Registration = 1,
    PeerListRequest,
    ChatMessage,
    SystemPing,
    MessageStatusUpdate
};

enum class MessageStatus : quint8 {
    Sending = 0,
    Sent,
    Delivered,
    Read,
    Error
};

/**
 * @brief Структура данных для одного пира (пользователя)
 */
struct PeerInfo{
    QString username;
    QString ip_address;
    quint16 port;
};

struct DataPacket{
    MessageType type;
    QByteArray data;
    QDateTime timestamp;
};

#endif // TYPES_H

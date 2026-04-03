#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QByteArray>
#include <QDateTime>

// ── Packet types ──────────────────────────────────────────────────────────────
enum class MessageType : quint8{
    Registration = 0,
    SystemNotification,
    Handshake,
    PeerListRequest,
    ReadReceipt,
    ChatMessage,
    SystemPing,
    MessageStatusUpdate
};

// ── Message delivery status ───────────────────────────────────────────────────
enum MessageStatus : quint8 {
    MsgPending = 0,   // saved locally, not yet sent
    MsgSent,          // delivered to peer or relay
    MsgFailed,        // relay returned error
    MsgRead,          // peer sent ReadReceipt
};

struct PeerInfo{
    QString username;
    QString ip_address;
    quint16 port;
};

// ── Raw packet exchanged between peers ───────────────────────────────────────
struct DataPacket{
    MessageType type;
    QByteArray data;
    QDateTime timestamp;
};

#endif // TYPES_H

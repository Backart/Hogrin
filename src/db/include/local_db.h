#ifndef LOCAL_DB_H
#define LOCAL_DB_H

#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QList>
#include <QDebug>
#include <QMetaType>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>

#include "types.h"

struct Message_Record {
    int       id            = -1;
    QString   peer;
    QString   sender;
    QString   text;
    QDateTime timestamp;
    bool      is_outgoing   = false;
    bool      is_delivered  = false;
    int       status        = MsgPending;   // MessageStatus enum
    int       reply_to_id   = -1;
    QString   reply_to_text;
    QString   reply_to_sender;
    bool      is_read       = false;
};

Q_DECLARE_METATYPE(Message_Record)

class Local_DB : public QObject
{
    Q_OBJECT

public:
    explicit Local_DB(QObject *parent = nullptr);
    ~Local_DB();

    bool init(const QString &nickname);

    // Returns inserted row ID, or -1 on failure
    int save_message(const QString &peer,
                     const QString &sender,
                     const QString &text,
                     const QDateTime &timestamp,
                     bool is_outgoing,
                     int  reply_to_id        = -1,
                     const QString &reply_to_text   = "",
                     const QString &reply_to_sender = "");

    QList<Message_Record> load_history(const QString &peer,
                                       int limit = 100) const;

    bool enqueue_message(const QString &peer, const QByteArray &encrypted_blob);
    QList<QByteArray> peek_outbox(const QString &peer);
    bool              confirm_outbox(const QString &peer);
    bool              has_pending(const QString &peer) const;

    bool mark_delivered(int message_id);

    // Status tracking
    bool update_status(int message_id, int status);

    // Read receipts — mark all outgoing messages to peer up to timestamp as Read
    bool mark_read_up_to(const QString &peer, qint64 up_to_timestamp);

    QStringList get_recent_chats() const;

    bool save_identity(const QByteArray &priv_key, const QByteArray &pub_key);
    bool load_identity(QByteArray &priv_key, QByteArray &pub_key) const;

    bool save_contact_key(const QString &peer, const QByteArray &pubkey);
    QByteArray get_contact_key(const QString &peer) const;

private:
    QSqlDatabase m_db;
    bool         m_ready = false;
    QString      m_connection_name;

    bool create_tables();
    void migrate_tables();
};

#endif // LOCAL_DB_H
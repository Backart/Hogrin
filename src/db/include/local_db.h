#ifndef LOCAL_DB_H
#define LOCAL_DB_H

#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QList>

struct Message_Record {
    int      id;
    QString  peer;
    QString  sender;
    QString  text;
    QDateTime timestamp;
    bool     is_outgoing;
    bool     is_delivered;
};

class Local_DB : public QObject
{
    Q_OBJECT

public:
    explicit Local_DB(QObject *parent = nullptr);
    ~Local_DB();

    bool init(const QString &nickname);

    bool save_message(const QString &peer,
                      const QString &sender,
                      const QString &text,
                      const QDateTime &timestamp,
                      bool is_outgoing);

    QList<Message_Record> load_history(const QString &peer,
                                       int limit = 100) const;

    bool enqueue_message(const QString &peer, const QByteArray &encrypted_blob);
    QList<QByteArray> dequeue_messages(const QString &peer);
    bool has_pending(const QString &peer) const;

    bool mark_delivered(int message_id);

private:
    QSqlDatabase m_db;
    bool         m_ready = false;

    bool create_tables();
    QString m_connection_name;
};

#endif // LOCAL_DB_H

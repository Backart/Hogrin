#include "local_db.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

Local_DB::Local_DB(QObject *parent)
    : QObject(parent)
{}

Local_DB::~Local_DB()
{
    if (m_db.isOpen())
        m_db.close();
    QSqlDatabase::removeDatabase("local_db");
}

bool Local_DB::init(const QString &nickname)
{
    // Путь к БД: ~/.local/share/Hogrin/<nickname>.db
    QString dir_path = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(dir_path);

    QString db_path = dir_path + "/" + nickname + ".db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", "local_db");
    m_db.setDatabaseName(db_path);

    if (!m_db.open()) {
        qWarning() << "Local_DB: failed to open:" << m_db.lastError().text();
        return false;
    }

    m_ready = create_tables();
    qDebug() << "Local_DB: initialized at" << db_path;
    return m_ready;
}

bool Local_DB::create_tables()
{
    QSqlQuery q(m_db);

    if (!q.exec("CREATE TABLE IF NOT EXISTS messages ("
                "id           INTEGER PRIMARY KEY AUTOINCREMENT,"
                "peer         TEXT    NOT NULL,"
                "sender       TEXT    NOT NULL,"
                "text         TEXT    NOT NULL,"
                "timestamp    INTEGER NOT NULL,"
                "is_outgoing  INTEGER NOT NULL DEFAULT 0,"
                "is_delivered INTEGER NOT NULL DEFAULT 0"
                ")")) {
        qWarning() << "Local_DB: create messages failed:" << q.lastError().text();
        return false;
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_messages_peer "
           "ON messages(peer)");

    if (!q.exec("CREATE TABLE IF NOT EXISTS outbox ("
                "id           INTEGER PRIMARY KEY AUTOINCREMENT,"
                "peer         TEXT NOT NULL,"
                "blob         BLOB NOT NULL,"
                "created_at   INTEGER NOT NULL"
                ")")) {
        qWarning() << "Local_DB: create outbox failed:" << q.lastError().text();
        return false;
    }

    return true;
}

bool Local_DB::save_message(const QString &peer,
                            const QString &sender,
                            const QString &text,
                            const QDateTime &timestamp,
                            bool is_outgoing)
{
    if (!m_ready) return false;

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO messages (peer, sender, text, timestamp, is_outgoing) "
              "VALUES (:peer, :sender, :text, :ts, :out)");
    q.bindValue(":peer",   peer);
    q.bindValue(":sender", sender);
    q.bindValue(":text",   text);
    q.bindValue(":ts",     timestamp.toSecsSinceEpoch());
    q.bindValue(":out",    is_outgoing ? 1 : 0);

    if (!q.exec()) {
        qWarning() << "Local_DB: save_message failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QList<Message_Record> Local_DB::load_history(const QString &peer,
                                             int limit) const
{
    QList<Message_Record> result;
    if (!m_ready) return result;

    QSqlQuery q(m_db);
    q.prepare("SELECT id, peer, sender, text, timestamp, is_outgoing, is_delivered "
              "FROM messages WHERE peer = :peer "
              "ORDER BY timestamp DESC LIMIT :limit");
    q.bindValue(":peer",  peer);
    q.bindValue(":limit", limit);
    q.exec();

    while (q.next()) {
        Message_Record msg;
        msg.id           = q.value(0).toInt();
        msg.peer         = q.value(1).toString();
        msg.sender       = q.value(2).toString();
        msg.text         = q.value(3).toString();
        msg.timestamp    = QDateTime::fromSecsSinceEpoch(q.value(4).toLongLong());
        msg.is_outgoing  = q.value(5).toBool();
        msg.is_delivered = q.value(6).toBool();
        result.prepend(msg);
    }

    return result;
}

bool Local_DB::mark_delivered(int message_id)
{
    if (!m_ready) return false;

    QSqlQuery q(m_db);
    q.prepare("UPDATE messages SET is_delivered = 1 WHERE id = :id");
    q.bindValue(":id", message_id);
    return q.exec();
}

bool Local_DB::enqueue_message(const QString &peer,
                               const QByteArray &encrypted_blob)
{
    if (!m_ready) return false;

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO outbox (peer, blob, created_at) "
              "VALUES (:peer, :blob, :ts)");
    q.bindValue(":peer", peer);
    q.bindValue(":blob", encrypted_blob);
    q.bindValue(":ts",   QDateTime::currentSecsSinceEpoch());

    if (!q.exec()) {
        qWarning() << "Local_DB: enqueue failed:" << q.lastError().text();
        return false;
    }

    qDebug() << "Local_DB: message enqueued for:" << peer;
    return true;
}

QList<QByteArray> Local_DB::dequeue_messages(const QString &peer)
{
    QList<QByteArray> result;
    if (!m_ready) return result;

    QSqlQuery q(m_db);
    q.prepare("SELECT blob FROM outbox WHERE peer = :peer ORDER BY id ASC");
    q.bindValue(":peer", peer);
    q.exec();

    while (q.next())
        result << q.value(0).toByteArray();

    QSqlQuery del(m_db);
    del.prepare("DELETE FROM outbox WHERE peer = :peer");
    del.bindValue(":peer", peer);
    del.exec();

    qDebug() << "Local_DB: dequeued" << result.size() << "messages for:" << peer;
    return result;
}

bool Local_DB::has_pending(const QString &peer) const
{
    if (!m_ready) return false;

    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM outbox WHERE peer = :peer");
    q.bindValue(":peer", peer);
    q.exec();
    q.next();
    return q.value(0).toInt() > 0;
}

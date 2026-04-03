#include "local_db.h"

Local_DB::Local_DB(QObject *parent) : QObject(parent) {}

Local_DB::~Local_DB()
{
    if (m_db.isOpen()) m_db.close();
    if (!m_connection_name.isEmpty()) QSqlDatabase::removeDatabase(m_connection_name);
}

bool Local_DB::init(const QString &nickname)
{
    if (m_db.isOpen()) {
        m_db.close();
        QSqlDatabase::removeDatabase(m_connection_name);
    }

    QString dir_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir_path);

    m_connection_name = "local_db_" + nickname;
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection_name);
    m_db.setDatabaseName(dir_path + "/" + nickname + ".db");

    if (!m_db.open()) {
        qWarning() << "Local_DB: failed to open:" << m_db.lastError().text();
        return false;
    }

    m_ready = create_tables();
    if (m_ready) migrate_tables();
    return m_ready;
}

bool Local_DB::create_tables()
{
    QSqlQuery q(m_db);
    if (!q.exec("CREATE TABLE IF NOT EXISTS messages ("
                "id               INTEGER PRIMARY KEY AUTOINCREMENT,"
                "peer             TEXT    NOT NULL,"
                "sender           TEXT    NOT NULL,"
                "text             TEXT    NOT NULL,"
                "timestamp        INTEGER NOT NULL,"
                "is_outgoing      INTEGER NOT NULL DEFAULT 0,"
                "is_delivered     INTEGER NOT NULL DEFAULT 0,"
                "status           INTEGER NOT NULL DEFAULT 1,"
                "reply_to_id      INTEGER NOT NULL DEFAULT -1,"
                "reply_to_text    TEXT    NOT NULL DEFAULT '',"
                "reply_to_sender  TEXT    NOT NULL DEFAULT '',"
                "is_read          INTEGER NOT NULL DEFAULT 0"
                ")")) {
        qWarning() << "Local_DB: create messages failed:" << q.lastError().text();
        return false;
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_messages_peer ON messages(peer)");

    q.exec("CREATE TABLE IF NOT EXISTS outbox ("
           "id         INTEGER PRIMARY KEY AUTOINCREMENT,"
           "peer       TEXT NOT NULL,"
           "blob       BLOB NOT NULL,"
           "created_at INTEGER NOT NULL)");

    q.exec("CREATE TABLE IF NOT EXISTS identity (key TEXT PRIMARY KEY, value BLOB)");
    q.exec("CREATE TABLE IF NOT EXISTS contacts (peer TEXT PRIMARY KEY, pubkey BLOB)");
    return true;
}

void Local_DB::migrate_tables()
{
    const QStringList alters = {
        "ALTER TABLE messages ADD COLUMN status           INTEGER NOT NULL DEFAULT 1",
        "ALTER TABLE messages ADD COLUMN reply_to_id      INTEGER NOT NULL DEFAULT -1",
        "ALTER TABLE messages ADD COLUMN reply_to_text    TEXT    NOT NULL DEFAULT ''",
        "ALTER TABLE messages ADD COLUMN reply_to_sender  TEXT    NOT NULL DEFAULT ''",
        "ALTER TABLE messages ADD COLUMN is_read          INTEGER NOT NULL DEFAULT 0",
    };
    for (const QString &sql : alters) {
        QSqlQuery aq(m_db);
        aq.exec(sql);  // silently fails if column already exists — that's fine
    }
}

// ── messages ──────────────────────────────────────────────────────────────────

int Local_DB::save_message(const QString &peer,
                           const QString &sender,
                           const QString &text,
                           const QDateTime &timestamp,
                           bool is_outgoing,
                           int  reply_to_id,
                           const QString &reply_to_text,
                           const QString &reply_to_sender)
{
    if (!m_ready) return -1;

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO messages "
              "(peer, sender, text, timestamp, is_outgoing, status, "
              " reply_to_id, reply_to_text, reply_to_sender) "
              "VALUES (:peer,:sender,:text,:ts,:out,:status,"
              "        :rid,:rtext,:rsender)");
    q.bindValue(":peer",    peer);
    q.bindValue(":sender",  sender);
    q.bindValue(":text",    text);
    q.bindValue(":ts",      timestamp.toSecsSinceEpoch());
    q.bindValue(":out",     is_outgoing ? 1 : 0);
    q.bindValue(":status",  MsgSent);
    q.bindValue(":rid",     reply_to_id);
    q.bindValue(":rtext",   reply_to_text);
    q.bindValue(":rsender", reply_to_sender);

    if (!q.exec()) {
        qWarning() << "Local_DB: save_message failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

QList<Message_Record> Local_DB::load_history(const QString &peer, int limit) const
{
    QList<Message_Record> result;
    if (!m_ready) return result;

    QSqlQuery q(m_db);
    q.prepare("SELECT id, peer, sender, text, timestamp, is_outgoing, is_delivered,"
              "       status, reply_to_id, reply_to_text, reply_to_sender, is_read "
              "FROM messages WHERE peer = :peer ORDER BY timestamp DESC LIMIT :limit");
    q.bindValue(":peer",  peer);
    q.bindValue(":limit", limit);
    q.exec();

    while (q.next()) {
        Message_Record m;
        m.id              = q.value(0).toInt();
        m.peer            = q.value(1).toString();
        m.sender          = q.value(2).toString();
        m.text            = q.value(3).toString();
        m.timestamp       = QDateTime::fromSecsSinceEpoch(q.value(4).toLongLong());
        m.is_outgoing     = q.value(5).toBool();
        m.is_delivered    = q.value(6).toBool();
        m.status          = q.value(7).toInt();
        m.reply_to_id     = q.value(8).toInt();
        m.reply_to_text   = q.value(9).toString();
        m.reply_to_sender = q.value(10).toString();
        m.is_read         = q.value(11).toBool();
        result.prepend(m);
    }
    return result;
}

bool Local_DB::mark_delivered(int id)  { return update_status(id, MsgSent); }

bool Local_DB::update_status(int message_id, int status)
{
    if (!m_ready || message_id < 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE messages SET status = :s WHERE id = :id");
    q.bindValue(":s",  status);
    q.bindValue(":id", message_id);
    return q.exec();
}

bool Local_DB::mark_read_up_to(const QString &peer, qint64 up_to_timestamp)
{
    if (!m_ready) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE messages SET status = :r, is_read = 1 "
              "WHERE peer = :peer AND is_outgoing = 1 "
              "  AND timestamp <= :ts AND status != :r");
    q.bindValue(":r",    MsgRead);
    q.bindValue(":peer", peer);
    q.bindValue(":ts",   up_to_timestamp);
    return q.exec();
}

// ── outbox ────────────────────────────────────────────────────────────────────

bool Local_DB::enqueue_message(const QString &peer, const QByteArray &blob)
{
    if (!m_ready) return false;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO outbox (peer, blob, created_at) VALUES (:p,:b,:ts)");
    q.bindValue(":p",  peer);
    q.bindValue(":b",  blob);
    q.bindValue(":ts", QDateTime::currentSecsSinceEpoch());
    return q.exec();
}

QList<QByteArray> Local_DB::peek_outbox(const QString &peer)
{
    QList<QByteArray> r;
    if (!m_ready) return r;
    QSqlQuery q(m_db);
    q.prepare("SELECT blob FROM outbox WHERE peer = :p ORDER BY id ASC");
    q.bindValue(":p", peer);
    q.exec();
    while (q.next()) r << q.value(0).toByteArray();
    return r;
}

bool Local_DB::confirm_outbox(const QString &peer)
{
    if (!m_ready) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM outbox WHERE peer = :p");
    q.bindValue(":p", peer);
    return q.exec();
}

bool Local_DB::has_pending(const QString &peer) const
{
    if (!m_ready) return false;
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM outbox WHERE peer = :p");
    q.bindValue(":p", peer);
    q.exec(); q.next();
    return q.value(0).toInt() > 0;
}

// ── chats ─────────────────────────────────────────────────────────────────────

QStringList Local_DB::get_recent_chats() const
{
    QStringList r;
    if (!m_db.isOpen()) return r;
    QSqlQuery q(m_db);
    q.prepare("SELECT DISTINCT peer FROM messages WHERE peer != '' ORDER BY timestamp DESC");
    if (q.exec()) while (q.next()) r << q.value(0).toString();
    return r;
}

// ── identity / contacts ───────────────────────────────────────────────────────

bool Local_DB::save_identity(const QByteArray &pub, const QByteArray &sec)
{
    if (!m_ready) return false;
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO identity (key,value) VALUES ('pubkey',:p),('seckey',:s)");
    q.bindValue(":p", pub); q.bindValue(":s", sec);
    return q.exec();
}

bool Local_DB::load_identity(QByteArray &pub, QByteArray &sec) const
{
    if (!m_ready) return false;
    QSqlQuery q(m_db);
    q.exec("SELECT value FROM identity WHERE key='pubkey'");
    if (q.next()) pub = q.value(0).toByteArray(); else return false;
    q.exec("SELECT value FROM identity WHERE key='seckey'");
    if (q.next()) sec = q.value(0).toByteArray(); else return false;
    return true;
}

bool Local_DB::save_contact_key(const QString &peer, const QByteArray &pubkey)
{
    if (!m_ready) return false;
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO contacts (peer,pubkey) VALUES (:p,:k)");
    q.bindValue(":p", peer); q.bindValue(":k", pubkey);
    return q.exec();
}

QByteArray Local_DB::get_contact_key(const QString &peer) const
{
    if (!m_ready) return {};
    QSqlQuery q(m_db);
    q.prepare("SELECT pubkey FROM contacts WHERE peer=:p");
    q.bindValue(":p", peer);
    if (q.exec() && q.next()) return q.value(0).toByteArray();
    return {};
}
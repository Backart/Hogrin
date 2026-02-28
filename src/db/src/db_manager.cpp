#include "db_manager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

DB_Manager::DB_Manager(QObject *parent)
    : QObject(parent)
{
    m_db = QSqlDatabase::addDatabase("QPSQL", "hogrin_connection");
}

DB_Manager::~DB_Manager()
{
    disconnect();
    QSqlDatabase::removeDatabase("hogrin_connection");
}

bool DB_Manager::connect(const QString &host, int port,
                         const QString &dbName,
                         const QString &user,
                         const QString &password)
{
    m_db.setHostName(host);
    m_db.setPort(port);
    m_db.setDatabaseName(dbName);
    m_db.setUserName(user);
    m_db.setPassword(password);

    m_connected = m_db.open();

    if (!m_connected)
        qWarning() << "DB connection failed:" << m_db.lastError().text();
    else
        qDebug() << "DB connected successfully.";

    return m_connected;
}

bool DB_Manager::isConnected() const
{
    return m_connected && m_db.isOpen();
}

void DB_Manager::disconnect()
{
    if (m_db.isOpen())
        m_db.close();
    m_connected = false;
}

// ── Users ────────────────────────────────────────────────────

bool DB_Manager::registerUser(const QString &nickname, const QString &passwordHash)
{
    if (userExists(nickname)) {
        qWarning() << "User already exists:" << nickname;
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO users (nickname, password_hash) VALUES (:nick, :hash)");
    query.bindValue(":nick", nickname);
    query.bindValue(":hash", passwordHash);

    if (!query.exec()) {
        qWarning() << "registerUser failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DB_Manager::userExists(const QString &nickname)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT 1 FROM users WHERE nickname = :nick LIMIT 1");
    query.bindValue(":nick", nickname);
    query.exec();
    return query.next();
}

bool DB_Manager::validateUser(const QString &nickname, const QString &passwordHash)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT 1 FROM users WHERE nickname = :nick AND password_hash = :hash AND is_banned = FALSE LIMIT 1");
    query.bindValue(":nick", nickname);
    query.bindValue(":hash", passwordHash);
    query.exec();
    return query.next();
}

bool DB_Manager::updateLastSeen(const QString &nickname)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE users SET last_seen = NOW() WHERE nickname = :nick");
    query.bindValue(":nick", nickname);
    return query.exec();
}

std::optional<UserRecord> DB_Manager::getUser(const QString &nickname)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id, nickname, password_hash, created_at, last_seen, is_banned "
                  "FROM users WHERE nickname = :nick LIMIT 1");
    query.bindValue(":nick", nickname);

    if (!query.exec() || !query.next())
        return std::nullopt;

    UserRecord user;
    user.id           = query.value(0).toInt();
    user.nickname     = query.value(1).toString();
    user.passwordHash = query.value(2).toString();
    user.createdAt    = query.value(3).toDateTime();
    user.lastSeen     = query.value(4).toDateTime();
    user.isBanned     = query.value(5).toBool();
    return user;
}

// ── Sessions ─────────────────────────────────────────────────

bool DB_Manager::createSession(const QString &nickname,
                               const QString &ip,
                               const QString &tokenHash)
{
    auto user = getUser(nickname);
    if (!user) return false;

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO sessions (user_id, ip_address, token_hash) "
                  "VALUES (:uid, :ip::inet, :token)");
    query.bindValue(":uid",   user->id);
    query.bindValue(":ip",    ip);
    query.bindValue(":token", tokenHash);

    if (!query.exec()) {
        qWarning() << "createSession failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DB_Manager::removeSession(const QString &tokenHash)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM sessions WHERE token_hash = :token");
    query.bindValue(":token", tokenHash);
    return query.exec();
}

bool DB_Manager::sessionExists(const QString &tokenHash)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT 1 FROM sessions WHERE token_hash = :token LIMIT 1");
    query.bindValue(":token", tokenHash);
    query.exec();
    return query.next();
}

void DB_Manager::cleanOldSessions()
{
    // Удаляем сессии старше 24 часов
    QSqlQuery query(m_db);
    query.exec("DELETE FROM sessions WHERE connected_at < NOW() - INTERVAL '24 hours'");
}

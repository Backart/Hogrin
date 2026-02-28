#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QDateTime>
#include "types.h"

struct UserRecord {
    int      id;
    QString  nickname;
    QString  passwordHash;
    QDateTime createdAt;
    QDateTime lastSeen;
    bool     isBanned;
};

class DB_Manager : public QObject {
    Q_OBJECT

public:
    explicit DB_Manager(QObject *parent = nullptr);
    ~DB_Manager();

    // Подключение
    bool connect(const QString &host,
                 int            port,
                 const QString &dbName,
                 const QString &user,
                 const QString &password);
    bool isConnected() const;
    void disconnect();

    // Users
    bool    registerUser(const QString &nickname, const QString &passwordHash);
    bool    userExists(const QString &nickname);
    bool    validateUser(const QString &nickname, const QString &passwordHash);
    bool    updateLastSeen(const QString &nickname);
    std::optional<UserRecord> getUser(const QString &nickname);

    // Sessions
    bool    createSession(const QString &nickname, const QString &ip, const QString &tokenHash);
    bool    removeSession(const QString &tokenHash);
    bool    sessionExists(const QString &tokenHash);
    void    cleanOldSessions();

private:
    QSqlDatabase m_db;
    bool         m_connected = false;

    bool execQuery(const QString &sql);
};

#endif // DB_MANAGER_H

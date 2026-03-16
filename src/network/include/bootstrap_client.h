#ifndef BOOTSTRAP_CLIENT_H
#define BOOTSTRAP_CLIENT_H

#include <QObject>
#include <QDebug>
#include <QTcpSocket>
#include <QQueue>
#include <QTimer>
#include <QDateTime>

#include "../../common/config.h"

class Bootstrap_Client : public QObject {
    Q_OBJECT
public:
    explicit Bootstrap_Client(QObject *parent = nullptr);

    void register_user(const QString &nickname, quint16 port, const QByteArray &public_key);
    void find_user(const QString &nickname);
    void unregister_user(const QString &nickname);
    bool store_message(const QString &nickname, const QByteArray &encrypted_blob);
    void fetch_messages(const QString &nickname);

    void auth_register(const QString &nickname, const QString &password);
    void auth_login   (const QString &nickname, const QString &password);
    void auth_verify  (const QString &token);
    void auth_logout  (const QString &token);

    void ensure_connected();

signals:
    void user_not_found(const QString &nickname);
    void user_found(const QString &nickname, const QString &host,
                    quint16 port, const QByteArray &peer_public_key);

    void messages_fetched(const QList<QByteArray> &messages);
    void store_confirmed();
    void store_failed(const QString &reason);

    void auth_register_success();
    void auth_register_failed(const QString &reason);
    void auth_login_success(const QString &token, const QString &nickname);
    void auth_login_failed(const QString &reason);
    void auth_verify_success(const QString &nickname);
    void auth_verify_failed(const QString &reason);
    void auth_logout_success();

    void registered(const QString &public_ip);

private:
    QTcpSocket  *m_socket;
    QQueue<QString> m_queue;
    QTimer      *m_reconnect_timer;
    bool         m_connecting = false;
    QDateTime    m_last_response_time;

    QString m_pending_find_nickname;
    QString m_pending_auth_nickname;

    void enqueue(const QString &message);

    void flush_queue();
    void on_connected();
    void on_disconnected();
    void parse_response(const QString &response);
};

#endif // BOOTSTRAP_CLIENT_H

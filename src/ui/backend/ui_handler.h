#ifndef UI_HANDLER_H
#define UI_HANDLER_H

#include <QObject>
#include <QCryptographicHash>
#include <QSettings>
#include <QUuid>

#include "../core/include/messenger_core.h"

class UI_Handler: public QObject{
    Q_OBJECT

public:

    explicit UI_Handler(Messenger_Core *core, QObject *parent = nullptr);

    Q_INVOKABLE void send_message_from_ui(const QString &username, const QString &text);
    Q_INVOKABLE bool start_server(quint16 port);
    Q_INVOKABLE void connect_to_host(const QString &host, quint16 port);

    Q_INVOKABLE void register_user(const QString &nickname, const QString &password);
    Q_INVOKABLE void login_user(const QString &nickname, const QString &password);

    Q_INVOKABLE void check_saved_session();
    Q_INVOKABLE void logout();

    Q_INVOKABLE void register_on_bootstrap(const QString &nickname);
    Q_INVOKABLE void find_peer(const QString &nickname);

    void unregister_from_bootstrap(const QString &nickname);

    Q_INVOKABLE QVariantList load_history(const QString &peer, int limit = 100);
    Q_INVOKABLE QStringList get_recent_chats();

    quint16 get_listening_port() const;

    Q_PROPERTY(QString connectionMode READ connectionMode NOTIFY connectionModeChanged)
    QString connectionMode() const { return m_connection_mode; }

private:

    Messenger_Core *m_core;

    QString m_connection_mode = "—";

signals:
    void message_received(QString username, QString text, QDateTime time);
    void session_restored(QString nickname);

    void peer_status_changed(QString nickname, bool isOnline);

    void peer_found(QString host, quint16 port);
    void peer_not_found();

    void relay_mode_activated();

    void login_result(bool success, const QString &error);
    void register_result(bool success, const QString &error);

    void connectionModeChanged();

public slots:
    void onRelayModeActivated() {
        m_connection_mode = "Relay e2ee IPv6";
        emit connectionModeChanged();
    }
    void onPeerFound(const QString&, const QString&, quint16) {
        m_connection_mode = "P2P e2ee IPv6";
        emit connectionModeChanged();
    }



};

#endif // UI_HANDLER_H

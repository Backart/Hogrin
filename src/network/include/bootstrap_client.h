#ifndef BOOTSTRAP_CLIENT_H
#define BOOTSTRAP_CLIENT_H

#include <QObject>
#include <QTcpSocket>

class Bootstrap_Client : public QObject {
    Q_OBJECT
public:
    explicit Bootstrap_Client(QObject *parent = nullptr);
    void register_user(const QString &nickname,
                       quint16 port,
                       const QByteArray &public_key);
    void find_user(const QString &nickname);
    void unregister_user(const QString &nickname);

signals:
    void user_not_found(const QString &nickname);
    void user_found(const QString &nickname,
                    const QString &host,
                    quint16 port,
                    const QByteArray &peer_public_key);

private:
    QTcpSocket *m_socket;
    void connect_and_send(const QString &message);
};

#endif // BOOTSTRAP_CLIENT_H

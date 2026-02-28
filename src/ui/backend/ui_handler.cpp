#include "ui_handler.h"
#include <QSettings>
#include <QUuid>

UI_Handler::UI_Handler(Messenger_Core *core,QObject *parent)
    : QObject(parent)
    , m_core(core)
{
    connect(m_core, &Messenger_Core::message_received, this,
            [this](const QString &username, const QString &text){
            emit message_received(username, text, QDateTime::currentDateTime());
            });
}

void UI_Handler::send_message_from_ui(const QString &username, const QString &text){
    m_core->send_message(username, text);
}

bool UI_Handler::start_server(quint16 port){
    return m_core->start_server(port);
}

void UI_Handler::connect_to_host(const QString &host, quint16 port){
    m_core->connect_to_host(host, port);
}

bool UI_Handler::connect_to_database(const QString &host, int port,
                                   const QString &dbName,
                                   const QString &user,
                                   const QString &password)
{
    return m_core->connect_to_database(host, port, dbName, user, password);
}

static QString hashPassword(const QString &password) {
    return QString(QCryptographicHash::hash(
                       password.toUtf8(),
                       QCryptographicHash::Sha256
                       ).toHex());
}

bool UI_Handler::register_user(const QString &nickname, const QString &password)
{
    return m_core->register_user(nickname, hashPassword(password));
}

bool UI_Handler::login_user(const QString &nickname, const QString &password)
{
    if (!m_core->login_user(nickname, hashPassword(password)))
        return false;

    QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_core->create_session(nickname, token);

    QSettings settings("Hogrin", "Hogrin");
    settings.setValue("session/token", token);
    settings.setValue("session/nickname", nickname);

    return true;
}

bool UI_Handler::check_saved_session()
{
    QSettings settings("Hogrin", "Hogrin");
    QString token    = settings.value("session/token").toString();
    QString nickname = settings.value("session/nickname").toString();

    if (token.isEmpty() || nickname.isEmpty()) return false;
    if (!m_core->session_exists(token)) return false;

    m_core->update_last_seen(nickname);
    emit session_restored(nickname);
    return true;
}

void UI_Handler::logout()
{
    QSettings settings("Hogrin", "Hogrin");
    QString token = settings.value("session/token").toString();
    m_core->remove_session(token);
    settings.remove("session/token");
    settings.remove("session/nickname");
}

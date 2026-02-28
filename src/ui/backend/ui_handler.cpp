#include "ui_handler.h"

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

bool UI_Handler::connectToDatabase(const QString &host, int port,
                                   const QString &dbName,
                                   const QString &user,
                                   const QString &password)
{
    return m_core->connectToDatabase(host, port, dbName, user, password);
}

static QString hashPassword(const QString &password) {
    return QString(QCryptographicHash::hash(
                       password.toUtf8(),
                       QCryptographicHash::Sha256
                       ).toHex());
}

bool UI_Handler::registerUser(const QString &nickname, const QString &passwordHash)
{
    return m_core->registerUser(nickname, passwordHash);
}

bool UI_Handler::loginUser(const QString &nickname, const QString &passwordHash)
{
    return m_core->loginUser(nickname, passwordHash);
}

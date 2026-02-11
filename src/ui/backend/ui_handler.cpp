#include "ui_handler.h"

#include <QDebug>

UI_Handler::UI_Handler(Messenger_Core *core,QObject *parent)
    : QObject(parent)
    , m_core(core)
{
    connect(m_core, &Messenger_Core::message_received, this, &UI_Handler::display_message);
}

void UI_Handler::send_message_from_ui(const QString &text){
    m_core->send_message(text);
}

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include "../core/include/messenger_core.h"


using namespace Qt::StringLiterals;


int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    const QUrl url(u"qrc:/qt/qml/Hogrin/src/ui/qml/Main.qml"_s);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);

    // --- НАЧАЛО ТЕСТА СЕТИ ---

    Messenger_Core core;

    core.start_server(1234);

    // 2. Теперь стучимся сами к себе (Клиент)
    // Подключаемся к локалхосту на тот же порт, который только что открыли
    core.connect_to_host("127.0.0.1", 1234);

    // 3. Таймер: даем время на рукопожатие (3 секунды) и отправляем данные
    QTimer::singleShot(3000, &core, [&](){
        qDebug() << "TEST: Sending data...";
        core.send_message("Hello from qt main.cpp!\n");
    });

    // --- КОНЕЦ ТЕСТА СЕТИ ---

    return app.exec();
}

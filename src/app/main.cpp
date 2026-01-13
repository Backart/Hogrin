#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include "../network/include/network_manager.h"

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

    // Создаем менеджер
    Network_Manager net;
    net.connect_to_host("127.0.0.1", 1234);

    // Таймер: через 3 секунды отправит привет в сокет
    QTimer::singleShot(3000, &net, [&](){
        net.send_data("Hello\n");
    });

    return app.exec();
}

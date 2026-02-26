#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QQmlContext>
#include "../core/include/messenger_core.h"
#include "../ui/backend/ui_handler.h"


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


    Messenger_Core core;
    UI_Handler ui_backend(&core);

    engine.rootContext()->setContextProperty("backend", &ui_backend);



    engine.load(url);

    return app.exec();
}

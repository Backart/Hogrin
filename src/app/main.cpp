#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>

#include "../core/include/messenger_core.h"
#include "../ui/backend/ui_handler.h"
#include "../common/config.h"
#include "../updater/update_checker.h"

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QFontDatabase::addApplicationFont(":/qt/qml/assets/fonts/NotoEmoji-Regular.ttf");

    qRegisterMetaType<Message_Record>();
    qRegisterMetaType<QList<Message_Record>>();

    QQmlApplicationEngine engine;

    const QUrl url(u"qrc:/qt/qml/Hogrin/src/ui/qml/Main.qml"_s);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    Messenger_Core core;
    UI_Handler ui_backend(&core);
    Update_Checker updater;

    engine.rootContext()->setContextProperty("backend", &ui_backend);
    engine.rootContext()->setContextProperty("updateChecker", &updater);

    engine.load(url);

    return app.exec();
}

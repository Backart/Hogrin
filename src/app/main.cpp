#include <QGuiApplication>
#include <QQmlApplicationEngine>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    // Самый надежный способ для Qt 6: загрузка через URI модуля и имя файла
    const QUrl url(u"qrc:/qt/qml/Hogrin/src/ui/qml/Main.qml"_s);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}

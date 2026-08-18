#include <QIcon>
#include <QObject>
#include <QQmlEngine>
#include <QApplication>
#include <QQuickWindow>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QScopedPointer>

#include <engine/engine.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/MTGScanner/icons/app.svg"));

    QCoreApplication::setOrganizationName("MadStudio");
    QCoreApplication::setOrganizationDomain("MadStudio.io");
    QCoreApplication::setApplicationName("MTGScanner");

    QScopedPointer<MTGS::Engine> mtgs_engine(new MTGS::Engine);
    qmlRegisterSingletonInstance("MTGScanner.Engine", 0, 1, "Engine", mtgs_engine.get());

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("MTGScanner", "Main");

    QObject *root_obj = engine.rootObjects().at(0);
    QQuickWindow *window = qobject_cast<QQuickWindow*>(root_obj);
    mtgs_engine->initializeOutputWindows(window);

    return app.exec();
}

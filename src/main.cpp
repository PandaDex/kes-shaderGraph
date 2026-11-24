#include "KesStyle.h"
#include "MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QSurfaceFormat>

int main(int argc, char** argv)
{
    // The preview runs Slang's GLSL 4.50 output. Set before the app starts so
    // every GL widget shares one format and docks can float without losing it.
    QSurfaceFormat format;
    format.setVersion(4, 5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Qt's native Windows file dialogs keep the main thread blocked for seconds after closing.
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QApplication::setApplicationName("kesGraph");
    QApplication::setOrganizationName("kesGraph");
    QApplication::setDesktopFileName("kesShaderGraph");
    QApplication::setWindowIcon(QIcon(":/icons/icon.png"));
    kes::style::apply(app);

    sne::MainWindow w;
    w.show();
    if (argc > 1) w.openFile(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}

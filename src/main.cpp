#include "MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QPalette>
#include <QSurfaceFormat>
#include <QStyleFactory>

static void applyDarkTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    const QColor window("#2b2b30"), base("#1f1f23"), text("#e2e2e8"), accent("#ffb43c");
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, QColor("#3a3a42"));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor("#38383f"));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor("#6fb1ff"));
    p.setColor(QPalette::Highlight, accent.darker(130));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor("#7d7d88"));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor("#6d6d78"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#6d6d78"));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#6d6d78"));
    app.setPalette(p);
    app.setStyleSheet("QToolTip { color:#e2e2e8; background:#3a3a42; border:1px solid #1b1b1f; padding:3px; }"
                      "QDockWidget::title { padding:5px; background:#33333a; }");
}

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

    QApplication app(argc, argv);
    QApplication::setApplicationName("kesGraph");
    QApplication::setOrganizationName("kesGraph");
    QApplication::setApplicationDisplayName("Slang Node Editor");
    QApplication::setDesktopFileName("kesShaderGraph");
    QApplication::setWindowIcon(QIcon(":/icons/icon.png"));
    applyDarkTheme(app);

    sne::MainWindow w;
    w.show();
    if (argc > 1) w.openFile(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}

#include "KesStyle.h"

#include <QApplication>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QPolygonF>
#include <QRegularExpression>
#include <QStyleFactory>

namespace kes::style {

namespace {

const QHash<QString, QString>& tokens()
{
    static const QHash<QString, QString> t = {
        { "bg0", "#161618" },      // separators, deepest frame
        { "base", "#1d1d20" },     // inputs, lists, text views
        { "alt", "#222225" },      // alternate rows
        { "title", "#242427" },    // dock titles, status bar
        { "bar", "#29292c" },      // menu bar, toolbars, headers
        { "window", "#2d2d30" },
        { "panel", "#303034" },    // section headers, raised panels
        { "card", "#2b2b2e" },
        { "raised", "#38383c" },   // buttons
        { "hover", "#404045" },
        { "line", "#3a3a3f" },
        { "border", "#4a4a50" },
        { "text", "#dcdce0" },
        { "bright", "#f0f0f2" },
        { "muted", "#9a9aa2" },
        { "disabled", "#6a6a70" },
        { "accent", "#f0963a" },
        { "accentStrong", "#b8681d" },
        { "accentDim", "#8a5420" },
        { "accentBg", "#4a3624" },
        { "accentText", "#ffe2c4" },
        { "warning", "#f0c040" },
        { "error", "#f06060" },
        { "info", "#5aa5ff" },
    };
    return t;
}

QColor tok(const char* name) { return QColor(tokens().value(QLatin1String(name))); }

const char* kStyleSheet = R"(
* { outline: 0; }
QWidget { font-size: 9pt; }
QToolTip { background: @bg0; color: @bright; border: 1px solid @accent; padding: 4px 6px; border-radius: 3px; }

QMainWindow::separator { background: @bg0; width: 4px; height: 4px; }
QMainWindow::separator:hover { background: @accent; }
QDockWidget { titlebar-close-icon: none; titlebar-normal-icon: none; font-weight: 600; color: #cfcfd4; }
QDockWidget::title { background: @title; padding: 6px 10px; border-bottom: 1px solid @bg0; text-align: left; }
QDialog { background: @window; }

QTabWidget::pane { border: 1px solid @base; top: -1px; }
QTabBar::tab { background: @bar; color: #a8a8ae; padding: 6px 14px; border: 1px solid @base; border-bottom: none; margin-right: 1px; }
QTabBar::tab:selected { background: @raised; color: @bright; border-top: 2px solid @accent; }
QTabBar::tab:hover:!selected { background: #323236; color: @text; }

QMenuBar { background: @bar; padding: 2px; }
QMenuBar::item { padding: 4px 10px; border-radius: 3px; }
QMenuBar::item:selected { background: #3d3d42; }
QMenu { background: #2b2b2e; border: 1px solid @bg0; padding: 4px; }
QMenu::item { padding: 5px 28px 5px 22px; border-radius: 3px; }
QMenu::item:selected { background: @accentStrong; color: white; }
QMenu::item:disabled { color: @disabled; }
QMenu::separator { height: 1px; background: @line; margin: 4px 8px; }

QToolBar { background: @bar; border: none; border-bottom: 1px solid @bg0; spacing: 4px; padding: 4px 6px; }
QToolBar::separator { background: @line; width: 1px; height: 1px; margin: 4px 6px; }
QToolBar#toolStrip { border-bottom: none; border-right: 1px solid @bg0; padding: 6px 4px; spacing: 6px; }
QToolButton { border: 1px solid transparent; border-radius: 4px; padding: 4px 6px; color: @text; }
QToolButton:hover { background: #3b3b40; border-color: #525258; }
QToolButton:pressed { background: #2f2f33; }
QToolButton:checked { background: @accentBg; border-color: @accent; color: @accentText; }
QToolButton:disabled { color: @disabled; }
QToolButton::menu-indicator { image: none; width: 0px; }
QToolButton[listening="true"] { background: @accentBg; border-color: @accent; }

QPushButton { background: @raised; border: 1px solid @border; border-radius: 4px; padding: 5px 14px; color: #e4e4e8; }
QPushButton:hover { background: @hover; border-color: @accent; }
QPushButton:pressed { background: #2f2f33; }
QPushButton:default { border-color: @accent; }
QPushButton:disabled { color: @disabled; border-color: @line; }
QPushButton#KeyButton { background: @base; border: 1px solid #434348; border-radius: 3px; text-align: left; padding: 3px 24px 3px 8px; }
QPushButton#KeyButton:hover { border-color: #5a5a60; }
QPushButton#KeyButton[invalid="true"] { color: #ff8080; border-color: #8a3a3a; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
    background: @base; border: 1px solid #434348; border-radius: 3px; padding: 3px 5px; selection-background-color: @accentStrong; }
QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover { border-color: #5a5a60; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus { border-color: @accent; }
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled { color: @disabled; background: #232326; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView { background: #252528; border: 1px solid @bg0; selection-background-color: @accentStrong; padding: 2px; }

QCheckBox, QRadioButton { spacing: 6px; }
QCheckBox::indicator, QRadioButton::indicator { width: 13px; height: 13px; border: 1px solid #55555b; background: @base; }
QCheckBox::indicator { border-radius: 3px; }
QRadioButton::indicator { border-radius: 7px; }
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: @accent; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { background: @accent; border-color: @accent; }

QHeaderView::section { background: @bar; color: #b8b8be; border: none; border-right: 1px solid @base; border-bottom: 1px solid @base; padding: 4px 6px; }
QTreeView, QListView, QTreeWidget, QListWidget, QTableWidget, QTableView {
    background: @base; alternate-background-color: @alt; border: 1px solid @bg0; selection-background-color: @accentDim; selection-color: white; }
QTreeView::item, QListView::item { padding: 3px 2px; }
QTreeView::item:hover:!selected, QListView::item:hover:!selected { background: #2c2c30; }
QTreeView::item:selected, QListView::item:selected { background: @accentDim; color: white; }

QScrollArea { border: none; background: transparent; }
QScrollBar:vertical { background: @base; width: 11px; margin: 0; }
QScrollBar:horizontal { background: @base; height: 11px; margin: 0; }
QScrollBar::handle:vertical { background: #46464c; min-height: 24px; border-radius: 4px; margin: 2px; }
QScrollBar::handle:horizontal { background: #46464c; min-width: 24px; border-radius: 4px; margin: 2px; }
QScrollBar::handle:hover { background: #5e5e66; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QStatusBar { background: @title; color: #b8b8be; border-top: 1px solid @bg0; }
QStatusBar::item { border: none; }
QStatusBar QLabel { padding: 0 8px; border-left: 1px solid #34343a; }
QSplitter::handle { background: @bg0; }
QSplitter::handle:hover { background: @accent; }

QLabel#Muted, QLabel#hint { color: @muted; }
QLabel#Heading { color: #c8c8d2; font-weight: 600; margin-top: 8px; }
QLabel#Title { font-size: 14pt; font-weight: 600; }
QLabel#EmptyState { color: #7a7a80; font-size: 11pt; }
QLabel#Warning { color: @warning; }

QFrame#Section { background: #262629; border: 1px solid #34343a; border-radius: 3px; }
QFrame#SectionHeader { background: @panel; border: none; border-top-left-radius: 3px; border-top-right-radius: 3px; }
QLabel#SectionTitle { font-weight: 600; }
QFrame#Card { background: @card; border: 1px solid @line; border-radius: 3px; }
QFrame#Card[expanded="true"] { border-color: @border; }
QFrame#CardHeader { background: transparent; border: none; }
QFrame#ModifierRow { background: #232326; border: 1px solid #36363b; border-radius: 3px; }
QFrame#Divider { background: @line; max-height: 1px; min-height: 1px; border: none; }
QFrame#Popup { background: @window; border: 1px solid @border; }
)";

void drawGlyph(QPainter& p, const QString& n, const QColor& c)
{
    auto line = [&](qreal x1, qreal y1, qreal x2, qreal y2) { p.drawLine(QPointF(x1, y1), QPointF(x2, y2)); };
    QColor ink = c;
    auto dot = [&](qreal x, qreal y, qreal r = 0.9) {
        p.save(); p.setPen(Qt::NoPen); p.setBrush(ink); p.drawEllipse(QPointF(x, y), r, r); p.restore();
    };
    auto tint = [&](const QColor& col) { ink = col; QPen pen = p.pen(); pen.setColor(col); p.setPen(pen); };

    if (n == "plus")        { line(8, 3.5, 8, 12.5); line(3.5, 8, 12.5, 8); }
    else if (n == "trash")
    {
        line(3, 4.5, 13, 4.5);
        p.drawPolyline(QPolygonF({ {6.5, 4.5}, {6.5, 3}, {9.5, 3}, {9.5, 4.5} }));
        p.drawPolygon(QPolygonF({ {4.5, 4.5}, {5.3, 13}, {10.7, 13}, {11.5, 4.5} }));
        line(7.2, 7, 7.2, 11); line(8.8, 7, 8.8, 11);
    }
    else if (n == "up")     p.drawPolyline(QPolygonF({ {4, 10}, {8, 6}, {12, 10} }));
    else if (n == "down")   p.drawPolyline(QPolygonF({ {4, 6}, {8, 10}, {12, 6} }));
    else if (n == "right")  p.drawPolyline(QPolygonF({ {6, 4}, {10, 8}, {6, 12} }));
    else if (n == "copy")
    {
        p.drawRoundedRect(QRectF(5.5, 5.5, 7, 8), 1, 1);
        p.drawPolyline(QPolygonF({ {3.5, 10.5}, {3.5, 2.5}, {9.5, 2.5} }));
    }
    else if (n == "keyboard")
    {
        p.drawRoundedRect(QRectF(1.5, 4, 13, 8), 1.5, 1.5);
        p.save(); QPen pen = p.pen(); pen.setWidthF(1.7); p.setPen(pen);
        for (qreal x : { 4.2, 6.7, 9.2, 11.7 }) p.drawPoint(QPointF(x, 6.8));
        p.drawPoint(QPointF(4.2, 9.1)); p.drawPoint(QPointF(11.7, 9.1));
        p.restore();
        line(6.2, 9.1, 9.8, 9.1);
    }
    else if (n == "mouse")
    {
        p.drawRoundedRect(QRectF(4.5, 2, 7, 12), 3.5, 3.5);
        line(8, 2, 8, 6.5); line(4.5, 6.5, 11.5, 6.5);
    }
    else if (n == "gamepad")
    {
        p.drawRoundedRect(QRectF(1.5, 4.5, 13, 7), 3.5, 3.5);
        line(4.2, 8, 7, 8); line(5.6, 6.6, 5.6, 9.4);
        dot(10, 7.2, 1); dot(11.9, 8.9, 1);
    }
    else if (n == "capture") { p.drawEllipse(QPointF(8, 8), 5, 5); dot(8, 8, 2); }
    else if (n == "search")  { p.drawEllipse(QPointF(7, 7), 3.8, 3.8); line(9.9, 9.9, 13.5, 13.5); }
    else if (n == "new")
    {
        p.drawPolygon(QPolygonF({ {4, 2}, {9.5, 2}, {12, 4.5}, {12, 14}, {4, 14} }));
        p.drawPolyline(QPolygonF({ {9.5, 2}, {9.5, 4.5}, {12, 4.5} }));
    }
    else if (n == "open")
    {
        p.drawPolyline(QPolygonF({ {2, 13}, {2, 4}, {6, 4}, {7.5, 5.5}, {12.5, 5.5}, {12.5, 7} }));
        p.drawPolygon(QPolygonF({ {2, 13}, {4, 7}, {14.5, 7}, {12.5, 13} }));
    }
    else if (n == "save")
    {
        p.drawRoundedRect(QRectF(2.5, 2.5, 11, 11), 1.2, 1.2);
        p.drawRect(QRectF(5, 2.5, 6, 3.5));
        p.drawRect(QRectF(4.5, 8.5, 7, 5));
    }
    else if (n == "undo" || n == "redo")
    {
        if (n == "redo") { p.translate(16, 0); p.scale(-1, 1); }
        p.drawPolyline(QPolygonF({ {6, 3.5}, {3.5, 6}, {6, 8.5} }));
        QPainterPath path;
        path.moveTo(3.5, 6);
        path.cubicTo(12, 5, 13.5, 10, 9, 12);
        p.drawPath(path);
    }
    else if (n == "warning")
    {
        tint(warning());
        p.drawPolygon(QPolygonF({ {8, 2.5}, {14, 13}, {2, 13} }));
        line(8, 6.5, 8, 9.5);
        dot(8, 11.2);
    }
    else if (n == "error")
    {
        tint(error());
        p.drawEllipse(QPointF(8, 8), 6, 6);
        line(5.8, 5.8, 10.2, 10.2); line(10.2, 5.8, 5.8, 10.2);
    }
    else if (n == "info")
    {
        tint(info());
        p.drawEllipse(QPointF(8, 8), 6, 6);
        line(8, 7.3, 8, 11);
        dot(8, 5.2);
    }
}

QPixmap renderIcon(const QString& name, const QColor& color)
{
    constexpr qreal dpr = 2.0;
    QPixmap pm(QSize(16, 16) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    drawGlyph(p, name, color);
    return pm;
}

} // namespace

QColor accent()    { return tok("accent"); }
QColor accentDim() { return tok("accentDim"); }
QColor warning()   { return tok("warning"); }
QColor error()     { return tok("error"); }
QColor info()      { return tok("info"); }
QColor text()      { return tok("text"); }
QColor muted()     { return tok("muted"); }
QColor disabled()  { return tok("disabled"); }
QColor base()      { return tok("base"); }
QColor window()    { return tok("window"); }
QColor panel()     { return tok("panel"); }
QColor border()    { return tok("border"); }

QString windowTitle(const QString& document, const QString& tool)
{
    return QStringLiteral("%1[*] - %2 | KES Engine").arg(document, tool);
}

void apply(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window, window());
    p.setColor(QPalette::WindowText, text());
    p.setColor(QPalette::Base, base());
    p.setColor(QPalette::AlternateBase, tok("alt"));
    p.setColor(QPalette::ToolTipBase, tok("bg0"));
    p.setColor(QPalette::ToolTipText, tok("bright"));
    p.setColor(QPalette::Text, text());
    p.setColor(QPalette::Button, tok("raised"));
    p.setColor(QPalette::ButtonText, text());
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Link, accent());
    p.setColor(QPalette::Highlight, tok("accentStrong"));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(0x82, 0x82, 0x88));
    p.setColor(QPalette::Light, QColor(0x48, 0x48, 0x4e));
    p.setColor(QPalette::Midlight, tok("line"));
    p.setColor(QPalette::Mid, tok("bar"));
    p.setColor(QPalette::Dark, tok("bg0"));
    p.setColor(QPalette::Shadow, Qt::black);
    for (auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        p.setColor(QPalette::Disabled, role, disabled());
    app.setPalette(p);

    // Substitute @token names with their colours.
    QString sheet = QString::fromLatin1(kStyleSheet);
    static const QRegularExpression re(QStringLiteral("@([A-Za-z0-9]+)"));
    QString out;
    out.reserve(sheet.size());
    qsizetype last = 0;
    for (auto it = re.globalMatch(sheet); it.hasNext();)
    {
        const auto m = it.next();
        out += QStringView(sheet).mid(last, m.capturedStart() - last);
        out += tokens().value(m.captured(1), m.captured(0));
        last = m.capturedEnd();
    }
    out += QStringView(sheet).mid(last);
    app.setStyleSheet(out);
}

QIcon icon(const QString& name)
{
    static QHash<QString, QIcon> cache;
    auto it = cache.constFind(name);
    if (it != cache.constEnd()) return *it;

    QIcon result;
    result.addPixmap(renderIcon(name, QColor(0xd2, 0xd2, 0xd7)), QIcon::Normal);
    result.addPixmap(renderIcon(name, QColor(0x66, 0x66, 0x6a)), QIcon::Disabled);
    cache.insert(name, result);
    return result;
}

} // namespace kes::style

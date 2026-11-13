#pragma once
// Shared look for the KES Engine editors (Forge, Shader Graph, Input Mapping Editor).
// Each editor keeps an identical copy of KesStyle.h/.cpp; change all three together.
#include <QColor>
#include <QIcon>
#include <QString>

class QApplication;

namespace kes::style {

void apply(QApplication& app);

// "<document>[*] - <tool> | KES Engine"
QString windowTitle(const QString& document, const QString& tool);

QColor accent();
QColor accentDim();
QColor warning();
QColor error();
QColor info();
QColor text();
QColor muted();
QColor disabled();
QColor base();
QColor window();
QColor panel();
QColor border();

// Painted 16px line icons, so no resource files are needed:
// new open save undo redo plus trash copy up down right search capture
// keyboard mouse gamepad warning error info
QIcon icon(const QString& name);

} // namespace kes::style

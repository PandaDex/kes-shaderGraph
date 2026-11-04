#include "GraphItems.h"

#include "GraphModel.h"
#include "NodeGraphScene.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <algorithm>
#include <cmath>

namespace sne {

namespace {
constexpr qreal kTitleH = 26;
constexpr qreal kRowH = 22;
constexpr qreal kPad = 10;
constexpr qreal kParamH = 18;
constexpr qreal kMinWidth = 190;
constexpr qreal kLodSimple = 0.35;
constexpr qreal kLodText = 0.5;

NodeGraphScene* nodeScene(QGraphicsItem* item)
{
    return item ? qobject_cast<NodeGraphScene*>(item->scene()) : nullptr;
}
} // namespace

QColor portColor(PortType t)
{
    switch (t) {
    case PortType::Float: return QColor("#a0d468");
    case PortType::Float2: return QColor("#48cfad");
    case PortType::Float3: return QColor("#4fc1e9");
    case PortType::Float4: return QColor("#5d9cec");
    case PortType::Int: return QColor("#ac92ec");
    case PortType::Bool: return QColor("#ed5565");
    case PortType::Generic: return QColor("#c8c8c8");
    }
    return Qt::white;
}

QColor categoryColor(const QString& c)
{
    static const QHash<QString, QColor> map = {
        {"Input", QColor("#3f7f5f")},    {"Constant", QColor("#5f6f8f")}, {"Property", QColor("#8f6f3f")},
        {"Math", QColor("#4a6fa5")},     {"Vector", QColor("#6a5acd")},   {"UV", QColor("#2f8f8f")},
        {"Texture", QColor("#a0522d")},  {"Procedural", QColor("#8f4f8f")}, {"Color", QColor("#b0603f")},
        {"Custom", QColor("#7f7f3f")},   {"Output", QColor("#a03a3a")},
    };
    return map.value(c, QColor("#555566"));
}

PortItem::PortItem(NodeItem* node, bool isOutput, int index)
    : QGraphicsItem(node), m_node(node), m_output(isOutput), m_index(index)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setCursor(Qt::CrossCursor);
    setZValue(2);
}

void PortItem::setPortType(PortType t)
{
    if (m_type == t) return;
    m_type = t;
    update();
}

void PortItem::setConnected(bool c)
{
    if (m_connected == c) return;
    m_connected = c;
    update();
}

void PortItem::setDropState(int s)
{
    if (m_drop == s) return;
    m_drop = s;
    update();
}

void PortItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (m_drop == 0 && QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform()) < kLodSimple) return;
    p->setRenderHint(QPainter::Antialiasing, true);
    const QColor c = portColor(m_type);
    qreal r = (m_hover || m_drop == 1) ? 7.0 : 5.5;
    if (m_drop == -1) {
        p->setPen(QPen(QColor("#ff5555"), 2));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(QPointF(0, 0), 7.5, 7.5);
        return;
    }
    if (m_drop == 1) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, 60));
        p->drawEllipse(QPointF(0, 0), 9, 9);
    }
    p->setPen(QPen(c.darker(150), 1.5));
    p->setBrush(m_connected ? c : QColor("#26262b"));
    p->drawEllipse(QPointF(0, 0), r, r);
    if (!m_connected) {
        p->setPen(Qt::NoPen);
        p->setBrush(c);
        p->drawEllipse(QPointF(0, 0), r - 3.2, r - 3.2);
    }
}

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) { m_hover = true; update(); }
void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) { m_hover = false; update(); }

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { e->ignore(); return; }
    if (auto* s = nodeScene(this)) s->beginConnectionDrag(this, e->scenePos());
    e->accept();
}

void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
    if (auto* s = nodeScene(this)) s->updateConnectionDrag(e->scenePos());
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    if (auto* s = nodeScene(this)) s->endConnectionDrag(e->scenePos());
    e->accept();
}

NodeItem::NodeItem(GraphModel* model, int nodeId)
    : m_model(model), m_id(nodeId), m_def(model->def(nodeId))
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(0);

    m_font.setPointSizeF(9);
    m_smallFont.setPointSizeF(7.5);
    m_titleFont.setPointSizeF(9.5);
    m_titleFont.setBold(true);

    for (int i = 0; i < m_def->inputs.size(); ++i) m_inputs << new PortItem(this, false, i);
    for (int o = 0; o < m_def->outputs.size(); ++o) m_outputs << new PortItem(this, true, o);
    m_inTypes.resize(m_inputs.size());
    m_outTypes.resize(m_outputs.size());
    for (int i = 0; i < m_inputs.size(); ++i) {
        m_inTypes[i] = m_def->inputs[i].type;
        m_inputs[i]->setPortType(m_inTypes[i]);
    }
    for (int o = 0; o < m_outputs.size(); ++o) {
        m_outTypes[o] = m_def->outputs[o].type;
        m_outputs[o]->setPortType(m_outTypes[o]);
    }

    const QColor cc = categoryColor(m_def->category);
    QLinearGradient g(0, 0, 0, kTitleH);
    g.setColorAt(0, cc.lighter(115));
    g.setColorAt(1, cc.darker(110));
    m_titleBrush = QBrush(g);
    m_titleFlat = cc;

    setToolTip(m_def->description);
    refresh();
}

QString NodeItem::defaultText(int i) const
{
    const PortDef& port = m_def->inputs[i];
    if (!port.defaultExpr.isEmpty()) return QStringLiteral("mesh");
    const QVariantList v = m_model->inputDefault(m_id, i);
    if (port.type == PortType::Generic || dimension(port.type) <= 1)
        return v.isEmpty() ? "0" : QString::number(v[0].toDouble(), 'g', 4);
    const int n = std::max(1, dimension(m_inTypes[i] == PortType::Generic ? PortType::Float : m_inTypes[i]));
    QStringList parts;
    for (int k = 0; k < n; ++k) parts << QString::number(k < v.size() ? v[k].toDouble() : 0.0, 'g', 3);
    return "(" + parts.join(", ") + ")";
}

QString NodeItem::paramText(const ParamDef& p) const
{
    const QVariant v = m_model->param(m_id, p.name);
    switch (p.kind) {
    case ParamKind::Float: return QString("%1: %2").arg(p.name).arg(v.toDouble(), 0, 'g', 5);
    case ParamKind::Int: return QString("%1: %2").arg(p.name).arg(v.toInt());
    case ParamKind::Bool: return QString("%1: %2").arg(p.name, v.toBool() ? "on" : "off");
    case ParamKind::Color: return QString("%1:").arg(p.name);
    default: return QString("%1: %2").arg(p.name, v.toString());
    }
}

void NodeItem::refresh()
{
    prepareGeometryChange();
    m_paramLines.clear();
    m_paramColors.clear();
    for (const ParamDef& p : m_def->params) {
        m_paramLines << paramText(p);
        QColor col;
        if (p.kind == ParamKind::Color) {
            const QVariantList c = m_model->param(m_id, p.name).toList();
            auto ch = [&](int i) { return std::clamp(int((i < c.size() ? c[i].toDouble() : 1.0) * 255.0 + 0.5), 0, 255); };
            col = QColor(ch(0), ch(1), ch(2));
        }
        m_paramColors << col;
    }
    m_defaultTexts.clear();
    for (int i = 0; i < m_def->inputs.size(); ++i) m_defaultTexts << defaultText(i);
    layout();
    update();
}

void NodeItem::setTypes(const QVector<PortType>& inputTypes, const QVector<PortType>& outputTypes)
{
    m_inTypes = inputTypes;
    m_outTypes = outputTypes;
    for (int i = 0; i < m_inputs.size() && i < inputTypes.size(); ++i) m_inputs[i]->setPortType(inputTypes[i]);
    for (int o = 0; o < m_outputs.size() && o < outputTypes.size(); ++o) m_outputs[o]->setPortType(outputTypes[o]);
    updateTypeLabels();
    update();
}

void NodeItem::updateTypeLabels()
{
    const QFontMetricsF fs(m_smallFont);
    m_outTypeNames.clear();
    m_outTypeW.clear();
    m_outTypeColors.clear();
    for (int o = 0; o < m_outputs.size(); ++o) {
        const PortType t = m_outTypes.value(o, PortType::Float);
        const QString ty = slangTypeName(t);
        m_outTypeNames << ty;
        m_outTypeW << fs.horizontalAdvance(ty);
        m_outTypeColors << portColor(t).darker(105);
    }
}

void NodeItem::layout()
{
    const QFontMetricsF fm(m_font), fs(m_smallFont), ft(m_titleFont);
    qreal leftW = 0, rightW = 0;
    m_defaultX.clear();
    for (int i = 0; i < m_def->inputs.size(); ++i) {
        const qreal nameW = fm.horizontalAdvance(m_def->inputs[i].name);
        m_defaultX << kPad + 4 + nameW + 8;
        leftW = std::max(leftW, nameW + 10 + fs.horizontalAdvance(m_defaultTexts.value(i)));
    }
    for (int o = 0; o < m_def->outputs.size(); ++o)
        rightW = std::max(rightW, fm.horizontalAdvance(m_def->outputs[o].name) + 10 + fs.horizontalAdvance("float4"));
    qreal paramW = 0;
    for (const QString& s : m_paramLines) paramW = std::max(paramW, fm.horizontalAdvance(s) + 30);
    m_width = std::max({kMinWidth, leftW + rightW + 2 * kPad + 24, paramW + 2 * kPad, ft.horizontalAdvance(m_def->title) + 2 * kPad + 8});
    m_width = std::ceil(m_width / 10.0) * 10.0;

    const int rows = std::max<int>(1, std::max(m_inputs.size(), m_outputs.size()));
    m_height = kTitleH + 6 + rows * kRowH + 6 + (m_paramLines.isEmpty() ? 0 : m_paramLines.size() * kParamH + 6);

    m_shape = QPainterPath();
    m_shape.addRoundedRect(QRectF(0, 0, m_width, m_height), 7, 7);
    m_titlePath = QPainterPath();
    m_titlePath.moveTo(0, kTitleH);
    m_titlePath.arcTo(QRectF(0, 0, 14, 14), 180, -90);
    m_titlePath.arcTo(QRectF(m_width - 14, 0, 14, 14), 90, -90);
    m_titlePath.lineTo(m_width, kTitleH);
    m_titlePath.closeSubpath();

    updateTypeLabels();

    for (int i = 0; i < m_inputs.size(); ++i) m_inputs[i]->setPos(0, kTitleH + 6 + i * kRowH + kRowH / 2);
    for (int o = 0; o < m_outputs.size(); ++o) m_outputs[o]->setPos(m_width, kTitleH + 6 + o * kRowH + kRowH / 2);

    if (auto* s = nodeScene(this)) s->updateConnectionsFor(this);
}

QRectF NodeItem::boundingRect() const { return QRectF(-4, -4, m_width + 8, m_height + 8); }

void NodeItem::paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget*)
{
    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform());
    const QRectF body(0, 0, m_width, m_height);
    const bool selected = opt->state & QStyle::State_Selected;
    const QColor border = selected ? QColor("#ffb43c") : QColor("#1b1b1f");
    const qreal bw = selected ? 2.4 : 1.2;

    if (lod < kLodSimple) {
        p->setRenderHint(QPainter::Antialiasing, false);
        if (selected) {
            QPen pen(border, 2);
            pen.setCosmetic(true);
            p->setPen(pen);
        } else {
            p->setPen(Qt::NoPen);
        }
        p->setBrush(QColor("#2f2f36"));
        p->drawRect(body);
        p->setPen(Qt::NoPen);
        p->setBrush(m_titleFlat);
        p->drawRect(QRectF(0, 0, m_width, kTitleH));
        return;
    }

    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 60));
    p->drawRoundedRect(body.translated(2.5, 3.5), 7, 7);

    p->setBrush(QColor("#2f2f36"));
    p->drawPath(m_shape);

    p->setBrush(m_titleBrush);
    p->drawPath(m_titlePath);

    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(border, bw));
    p->drawPath(m_shape);

    p->setPen(QColor("#f4f4f6"));
    p->setFont(m_titleFont);
    p->drawText(QRectF(kPad, 0, m_width - 2 * kPad, kTitleH), Qt::AlignVCenter | Qt::AlignLeft, m_def->title);

    if (lod < kLodText) return;

    p->setFont(m_font);
    p->setPen(QColor("#dcdce2"));
    for (int i = 0; i < m_inputs.size(); ++i) {
        const qreal y = kTitleH + 6 + i * kRowH;
        p->drawText(QRectF(kPad + 4, y, m_width, kRowH), Qt::AlignVCenter | Qt::AlignLeft, m_def->inputs[i].name);
    }
    for (int o = 0; o < m_outputs.size(); ++o) {
        const qreal y = kTitleH + 6 + o * kRowH;
        p->drawText(QRectF(0, y, m_width - kPad - 4 - m_outTypeW.value(o) - 8, kRowH), Qt::AlignVCenter | Qt::AlignRight,
                    m_def->outputs[o].name);
    }

    p->setFont(m_smallFont);
    p->setPen(QColor("#8a8a96"));
    for (int i = 0; i < m_inputs.size(); ++i) {
        if (m_inputs[i]->isConnected()) continue;
        const qreal y = kTitleH + 6 + i * kRowH;
        p->drawText(QRectF(m_defaultX.value(i), y, m_width, kRowH), Qt::AlignVCenter | Qt::AlignLeft,
                    m_defaultTexts.value(i));
    }
    for (int o = 0; o < m_outputs.size(); ++o) {
        const qreal y = kTitleH + 6 + o * kRowH;
        p->setPen(m_outTypeColors.value(o));
        p->drawText(QRectF(0, y, m_width - kPad - 4, kRowH), Qt::AlignVCenter | Qt::AlignRight, m_outTypeNames.value(o));
    }

    if (!m_paramLines.isEmpty()) {
        const qreal y0 = kTitleH + 6 + std::max<int>(1, std::max(m_inputs.size(), m_outputs.size())) * kRowH + 2;
        p->setPen(QColor("#3d3d46"));
        p->drawLine(QPointF(kPad, y0), QPointF(m_width - kPad, y0));
        p->setFont(m_font);
        for (int k = 0; k < m_paramLines.size(); ++k) {
            const qreal y = y0 + 3 + k * kParamH;
            p->setPen(QColor("#b4b4c0"));
            p->drawText(QRectF(kPad + 4, y, m_width - 2 * kPad, kParamH), Qt::AlignVCenter | Qt::AlignLeft,
                        m_paramLines[k]);
            if (m_paramColors[k].isValid()) {
                p->setPen(QPen(QColor("#111"), 1));
                p->setBrush(m_paramColors[k]);
                p->drawRoundedRect(QRectF(m_width - kPad - 46, y + 2, 42, kParamH - 4), 3, 3);
            }
        }
    }
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange) {
        if (auto* s = nodeScene(this); s && s->snapToGrid()) {
            const QPointF pt = value.toPointF();
            constexpr qreal g = 20.0;
            return QPointF(std::round(pt.x() / g) * g, std::round(pt.y() / g) * g);
        }
    } else if (change == ItemPositionHasChanged) {
        if (auto* s = nodeScene(this)) s->updateConnectionsFor(this);
    }
    return QGraphicsItem::itemChange(change, value);
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    m_pressPos = pos();
    QGraphicsItem::mousePressEvent(e);
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    QGraphicsItem::mouseReleaseEvent(e);
    if (pos() != m_pressPos) {
        if (auto* s = nodeScene(this)) s->commitMoves();
    }
}

ConnectionItem::ConnectionItem(int id, PortItem* from, PortItem* to)
    : m_id(id), m_from(from), m_to(to)
{
    setFlags(ItemIsSelectable);
    setZValue(-1);
    setAcceptedMouseButtons(Qt::LeftButton);
    updatePath();
}

QPainterPath ConnectionItem::makePath(const QPointF& a, const QPointF& b)
{
    QPainterPath path(a);
    const qreal dx = std::max<qreal>(50.0, std::abs(b.x() - a.x()) * 0.5);
    path.cubicTo(a + QPointF(dx, 0), b - QPointF(dx, 0), b);
    return path;
}

void ConnectionItem::updatePath()
{
    const QPainterPath path = makePath(m_from->anchorScenePos(), m_to->anchorScenePos());
    QPainterPathStroker s;
    s.setWidth(12);
    m_shape = s.createStroke(path);
    prepareGeometryChange();
    setPath(path);
}

void ConnectionItem::paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget*)
{
    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform());
    const bool sel = opt->state & QStyle::State_Selected;
    QColor c = portColor(m_from->portType());
    if (sel) c = c.lighter(150);
    p->setBrush(Qt::NoBrush);
    if (lod < kLodSimple) {
        p->setRenderHint(QPainter::Antialiasing, false);
        QPen pen(c, sel ? 2 : 1);
        pen.setCosmetic(true);
        p->setPen(pen);
        p->drawPath(path());
        return;
    }
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(QPen(QColor(0, 0, 0, 90), sel ? 6 : 5, Qt::SolidLine, Qt::RoundCap));
    p->drawPath(path());
    p->setPen(QPen(c, sel ? 3.6 : 2.6, Qt::SolidLine, Qt::RoundCap));
    p->drawPath(path());
}

} // namespace sne

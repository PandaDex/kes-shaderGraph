#pragma once

#include "NodeLibrary.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QVector>

namespace sne {

class GraphModel;
class NodeGraphScene;
class NodeItem;

QColor portColor(PortType t);
QColor categoryColor(const QString& category);

class PortItem : public QGraphicsItem {
public:
    enum { Type = QGraphicsItem::UserType + 2 };

    PortItem(NodeItem* node, bool isOutput, int index);
    int type() const override { return Type; }

    NodeItem* node() const { return m_node; }
    bool isOutput() const { return m_output; }
    int index() const { return m_index; }
    bool isConnected() const { return m_connected; }

    PortType portType() const { return m_type; }
    void setPortType(PortType t);
    void setConnected(bool c);
    void setDropState(int s);

    QPointF anchorScenePos() const { return mapToScene(0, 0); }

    QRectF boundingRect() const override { return QRectF(-9, -9, 18, 18); }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;
    void mousePressEvent(QGraphicsSceneMouseEvent*) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;

private:
    NodeItem* m_node;
    bool m_output;
    int m_index;
    PortType m_type = PortType::Float;
    bool m_connected = false;
    bool m_hover = false;
    int m_drop = 0;
};

class NodeItem : public QGraphicsItem {
public:
    enum { Type = QGraphicsItem::UserType + 1 };

    NodeItem(GraphModel* model, int nodeId);
    int type() const override { return Type; }

    int nodeId() const { return m_id; }
    const NodeDef* nodeDef() const { return m_def; }
    PortItem* inputPort(int i) const { return m_inputs.value(i); }
    PortItem* outputPort(int i) const { return m_outputs.value(i); }
    const QVector<PortItem*>& inputPorts() const { return m_inputs; }
    const QVector<PortItem*>& outputPorts() const { return m_outputs; }

    void refresh();
    void setTypes(const QVector<PortType>& inputTypes, const QVector<PortType>& outputTypes);

    QRectF boundingRect() const override;
    QPainterPath shape() const override { return m_shape; }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent*) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;

private:
    void layout();
    void updateTypeLabels();
    QString defaultText(int inputIndex) const;
    QString paramText(const ParamDef& p) const;

    GraphModel* m_model;
    int m_id;
    const NodeDef* m_def;
    QVector<PortItem*> m_inputs;
    QVector<PortItem*> m_outputs;
    QVector<PortType> m_inTypes, m_outTypes;
    QStringList m_paramLines;
    QVector<QColor> m_paramColors;
    QStringList m_defaultTexts;
    QVector<qreal> m_defaultX;
    QStringList m_outTypeNames;
    QVector<qreal> m_outTypeW;
    QVector<QColor> m_outTypeColors;
    QFont m_font, m_titleFont, m_smallFont;
    QPainterPath m_shape;
    QPainterPath m_titlePath;
    QBrush m_titleBrush;
    QColor m_titleFlat;
    qreal m_width = 200;
    qreal m_height = 60;
    QPointF m_pressPos;
};

class ConnectionItem : public QGraphicsPathItem {
public:
    enum { Type = QGraphicsItem::UserType + 3 };

    ConnectionItem(int id, PortItem* from, PortItem* to);
    int type() const override { return Type; }

    int connectionId() const { return m_id; }
    PortItem* from() const { return m_from; }
    PortItem* to() const { return m_to; }

    void updatePath();
    static QPainterPath makePath(const QPointF& a, const QPointF& b);

    QPainterPath shape() const override { return m_shape; }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

private:
    int m_id;
    PortItem* m_from;
    PortItem* m_to;
    QPainterPath m_shape;
};

} // namespace sne

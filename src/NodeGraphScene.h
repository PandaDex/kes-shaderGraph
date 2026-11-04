#pragma once

#include "GraphItems.h"

#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QHash>

namespace sne {

class GraphModel;

class NodeGraphScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit NodeGraphScene(GraphModel* model, QObject* parent = nullptr);

    GraphModel* model() const { return m_model; }
    NodeItem* nodeItem(int id) const { return m_nodeItems.value(id); }
    QList<int> selectedNodeIds() const;
    QRectF nodesBoundingRect(bool selectedOnly = false) const;

    bool snapToGrid() const { return m_snap; }
    void setSnapToGrid(bool on) { m_snap = on; }

    int addNodeAt(const QString& type, const QPointF& pos);
    void createNodeAndConnect(const QString& type, const QPointF& pos, int fromNode, int fromPort, bool fromIsOutput);
    void deleteSelection();
    void copySelection();
    void cutSelection();
    void paste(const QPointF& scenePos);
    void duplicateSelection();
    void selectNodes(const QList<int>& ids);

    void beginConnectionDrag(PortItem* port, const QPointF& scenePos);
    void updateConnectionDrag(const QPointF& scenePos);
    void endConnectionDrag(const QPointF& scenePos);
    void commitMoves();
    void updateConnectionsFor(NodeItem* node);

signals:
    void nodePopupRequested(const QPointF& scenePos, int fromNode, int fromPort, bool fromIsOutput);
    void selectionIdsChanged(const QList<int>& ids);
    void statusMessage(const QString& text);

private slots:
    void onNodeAdded(int id);
    void onNodeRemoved(int id);
    void onNodeChanged(int id);
    void onNodePosChanged(int id);
    void onConnectionAdded(int id);
    void onConnectionRemoved(int id);
    void onGraphReset();
    void onModelChanged();
    void onSelectionChanged();

private:
    void rebuild();
    void refreshPortStates();
    void applyTypeInfo();
    PortItem* portAt(const QPointF& scenePos) const;
    bool canDrop(PortItem* anchor, PortItem* target) const;
    void clearDropHighlight();
    QString clipboardJson() const;

    GraphModel* m_model;
    QHash<int, NodeItem*> m_nodeItems;
    QHash<int, ConnectionItem*> m_connItems;
    QGraphicsPathItem* m_tempLine;

    PortItem* m_dragAnchor = nullptr;
    PortItem* m_hoverTarget = nullptr;
    int m_pickedConn = -1;
    bool m_dragging = false;
    bool m_rebuilding = false;
    bool m_snap = false;
    int m_pasteCount = 0;
};

} // namespace sne

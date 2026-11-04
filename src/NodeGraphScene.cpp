#include "NodeGraphScene.h"

#include "GraphModel.h"
#include "SlangCodeGen.h"

#include <QApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPen>
#include <QSet>
#include <algorithm>

namespace sne {

NodeGraphScene::NodeGraphScene(GraphModel* model, QObject* parent)
    : QGraphicsScene(parent), m_model(model)
{
    setSceneRect(-60000, -60000, 120000, 120000);
    setItemIndexMethod(QGraphicsScene::NoIndex);

    m_tempLine = new QGraphicsPathItem;
    m_tempLine->setZValue(10);
    m_tempLine->setAcceptedMouseButtons(Qt::NoButton);
    m_tempLine->setPen(QPen(QColor("#ffffff"), 2.4, Qt::DashLine, Qt::RoundCap));
    m_tempLine->setVisible(false);
    addItem(m_tempLine);

    connect(m_model, &GraphModel::nodeAdded, this, &NodeGraphScene::onNodeAdded);
    connect(m_model, &GraphModel::nodeRemoved, this, &NodeGraphScene::onNodeRemoved);
    connect(m_model, &GraphModel::nodeChanged, this, &NodeGraphScene::onNodeChanged);
    connect(m_model, &GraphModel::nodePosChanged, this, &NodeGraphScene::onNodePosChanged);
    connect(m_model, &GraphModel::connectionAdded, this, &NodeGraphScene::onConnectionAdded);
    connect(m_model, &GraphModel::connectionRemoved, this, &NodeGraphScene::onConnectionRemoved);
    connect(m_model, &GraphModel::graphReset, this, &NodeGraphScene::onGraphReset);
    connect(m_model, &GraphModel::changed, this, &NodeGraphScene::onModelChanged);
    connect(this, &QGraphicsScene::selectionChanged, this, &NodeGraphScene::onSelectionChanged);

    rebuild();
}

void NodeGraphScene::onNodeAdded(int id)
{
    if (m_nodeItems.contains(id)) return;
    auto* item = new NodeItem(m_model, id);
    if (const GraphNode* n = m_model->node(id)) item->setPos(n->pos);
    addItem(item);
    m_nodeItems.insert(id, item);
}

void NodeGraphScene::onNodeRemoved(int id)
{
    if (NodeItem* item = m_nodeItems.take(id)) {
        removeItem(item);
        delete item;
    }
}

void NodeGraphScene::onNodeChanged(int id)
{
    if (NodeItem* item = m_nodeItems.value(id)) item->refresh();
}

void NodeGraphScene::onNodePosChanged(int id)
{
    NodeItem* item = m_nodeItems.value(id);
    const GraphNode* n = m_model->node(id);
    if (item && n && item->pos() != n->pos) item->setPos(n->pos);
}

void NodeGraphScene::onConnectionAdded(int id)
{
    const GraphConnection* c = m_model->connection(id);
    if (!c || m_connItems.contains(id)) return;
    NodeItem* a = m_nodeItems.value(c->fromNode);
    NodeItem* b = m_nodeItems.value(c->toNode);
    if (!a || !b || !a->outputPort(c->fromPort) || !b->inputPort(c->toPort)) return;
    auto* item = new ConnectionItem(id, a->outputPort(c->fromPort), b->inputPort(c->toPort));
    addItem(item);
    m_connItems.insert(id, item);
}

void NodeGraphScene::onConnectionRemoved(int id)
{
    if (ConnectionItem* item = m_connItems.take(id)) {
        removeItem(item);
        delete item;
    }
}

void NodeGraphScene::onGraphReset()
{
    const QList<int> sel = selectedNodeIds();
    rebuild();
    selectNodes(sel);
}

void NodeGraphScene::rebuild()
{
    m_rebuilding = true;
    m_dragging = false;
    m_dragAnchor = nullptr;
    m_hoverTarget = nullptr;
    m_tempLine->setVisible(false);

    for (ConnectionItem* c : std::as_const(m_connItems)) { removeItem(c); delete c; }
    m_connItems.clear();
    for (NodeItem* n : std::as_const(m_nodeItems)) { removeItem(n); delete n; }
    m_nodeItems.clear();

    for (int id : m_model->nodeIds()) onNodeAdded(id);
    for (const GraphConnection& c : m_model->connections()) onConnectionAdded(c.id);
    m_rebuilding = false;
}

void NodeGraphScene::onModelChanged()
{
    if (m_rebuilding) return;
    refreshPortStates();
    applyTypeInfo();
}

void NodeGraphScene::refreshPortStates()
{
    QSet<quint64> usedOut, usedIn;
    for (const GraphConnection& c : m_model->connections()) {
        usedOut.insert(TypeInfo::key(c.fromNode, c.fromPort));
        usedIn.insert(TypeInfo::key(c.toNode, c.toPort));
    }
    for (NodeItem* n : std::as_const(m_nodeItems)) {
        for (PortItem* p : n->inputPorts()) p->setConnected(usedIn.contains(TypeInfo::key(n->nodeId(), p->index())));
        for (PortItem* p : n->outputPorts()) p->setConnected(usedOut.contains(TypeInfo::key(n->nodeId(), p->index())));
        n->update();
    }
}

void NodeGraphScene::applyTypeInfo()
{
    const TypeInfo ti = inferTypes(*m_model);
    for (NodeItem* n : std::as_const(m_nodeItems)) {
        const NodeDef* d = n->nodeDef();
        QVector<PortType> in(d->inputs.size()), out(d->outputs.size());
        for (int i = 0; i < in.size(); ++i) {
            const PortType t = d->inputs[i].type;
            in[i] = t == PortType::Generic ? ti.genericType(n->nodeId()) : t;
            if (d->role == NodeRole::CustomExpression) {
                if (const GraphConnection* c = m_model->inputConnection(n->nodeId(), i))
                    in[i] = ti.outputType(c->fromNode, c->fromPort);
                else
                    in[i] = PortType::Float;
            }
        }
        for (int o = 0; o < out.size(); ++o) out[o] = ti.outputType(n->nodeId(), o);
        n->setTypes(in, out);
        n->refresh();
    }
    for (ConnectionItem* c : std::as_const(m_connItems)) { c->updatePath(); c->update(); }
}

void NodeGraphScene::updateConnectionsFor(NodeItem* node)
{
    for (ConnectionItem* c : std::as_const(m_connItems))
        if (c->from()->node() == node || c->to()->node() == node) c->updatePath();
}

QList<int> NodeGraphScene::selectedNodeIds() const
{
    QList<int> ids;
    for (QGraphicsItem* it : selectedItems())
        if (it->type() == NodeItem::Type) ids << static_cast<NodeItem*>(it)->nodeId();
    std::sort(ids.begin(), ids.end());
    return ids;
}

void NodeGraphScene::selectNodes(const QList<int>& ids)
{
    blockSignals(true);
    clearSelection();
    for (int id : ids)
        if (NodeItem* n = m_nodeItems.value(id)) n->setSelected(true);
    blockSignals(false);
    onSelectionChanged();
}

void NodeGraphScene::onSelectionChanged()
{
    if (m_rebuilding) return;
    emit selectionIdsChanged(selectedNodeIds());
}

QRectF NodeGraphScene::nodesBoundingRect(bool selectedOnly) const
{
    QRectF r;
    for (NodeItem* n : m_nodeItems) {
        if (selectedOnly && !n->isSelected()) continue;
        r = r.isNull() ? n->sceneBoundingRect() : r.united(n->sceneBoundingRect());
    }
    return r;
}

PortItem* NodeGraphScene::portAt(const QPointF& scenePos) const
{
    PortItem* best = nullptr;
    qreal bestDist = 1e9;
    const QRectF probe(scenePos - QPointF(14, 14), QSizeF(28, 28));
    for (QGraphicsItem* it : items(probe, Qt::IntersectsItemBoundingRect)) {
        if (it->type() != PortItem::Type) continue;
        auto* p = static_cast<PortItem*>(it);
        const QPointF d = p->anchorScenePos() - scenePos;
        const qreal dist = d.x() * d.x() + d.y() * d.y();
        if (dist < bestDist) { bestDist = dist; best = p; }
    }
    return best;
}

bool NodeGraphScene::canDrop(PortItem* anchor, PortItem* target) const
{
    if (!anchor || !target) return false;
    if (anchor->isOutput() == target->isOutput()) return false;
    PortItem* out = anchor->isOutput() ? anchor : target;
    PortItem* in = anchor->isOutput() ? target : anchor;
    return m_model->canConnect(out->node()->nodeId(), out->index(), in->node()->nodeId(), in->index());
}

void NodeGraphScene::clearDropHighlight()
{
    if (m_hoverTarget) { m_hoverTarget->setDropState(0); m_hoverTarget = nullptr; }
}

void NodeGraphScene::beginConnectionDrag(PortItem* port, const QPointF& scenePos)
{
    m_dragAnchor = port;
    m_pickedConn = -1;
    if (!port->isOutput()) {
        if (const GraphConnection* c = m_model->inputConnection(port->node()->nodeId(), port->index())) {
            if (NodeItem* src = m_nodeItems.value(c->fromNode)) {
                m_pickedConn = c->id;
                m_dragAnchor = src->outputPort(c->fromPort);
                if (ConnectionItem* ci = m_connItems.value(c->id)) ci->setVisible(false);
            }
        }
    }
    m_dragging = true;
    m_tempLine->setVisible(true);
    updateConnectionDrag(scenePos);
}

void NodeGraphScene::updateConnectionDrag(const QPointF& scenePos)
{
    if (!m_dragging || !m_dragAnchor) return;
    const QPointF a = m_dragAnchor->anchorScenePos();
    m_tempLine->setPath(m_dragAnchor->isOutput() ? ConnectionItem::makePath(a, scenePos)
                                                 : ConnectionItem::makePath(scenePos, a));
    QPen pen = m_tempLine->pen();
    pen.setColor(portColor(m_dragAnchor->portType()));
    m_tempLine->setPen(pen);

    PortItem* target = portAt(scenePos);
    if (target != m_hoverTarget) {
        clearDropHighlight();
        if (target && target != m_dragAnchor && target->isOutput() != m_dragAnchor->isOutput()) {
            m_hoverTarget = target;
            target->setDropState(canDrop(m_dragAnchor, target) ? 1 : -1);
        }
    }
}

void NodeGraphScene::endConnectionDrag(const QPointF& scenePos)
{
    if (!m_dragging) return;
    PortItem* anchor = m_dragAnchor;
    const int picked = m_pickedConn;
    PortItem* target = portAt(scenePos);

    clearDropHighlight();
    m_dragging = false;
    m_dragAnchor = nullptr;
    m_pickedConn = -1;
    m_tempLine->setVisible(false);

    if (!anchor) return;
    if (picked >= 0)
        if (ConnectionItem* ci = m_connItems.value(picked)) ci->setVisible(true);

    if (target && target != anchor && canDrop(anchor, target)) {
        PortItem* out = anchor->isOutput() ? anchor : target;
        PortItem* in = anchor->isOutput() ? target : anchor;
        const int fn = out->node()->nodeId(), fp = out->index();
        const int tn = in->node()->nodeId(), tp = in->index();
        m_model->edit("Connect", [&] {
            if (picked >= 0) m_model->removeConnection(picked);
            m_model->addConnection(fn, fp, tn, tp);
        });
        return;
    }

    if (target) {
        QString why;
        if (target->isOutput() != anchor->isOutput()) {
            PortItem* out = anchor->isOutput() ? anchor : target;
            PortItem* in = anchor->isOutput() ? target : anchor;
            m_model->canConnect(out->node()->nodeId(), out->index(), in->node()->nodeId(), in->index(), &why);
        } else {
            why = "Connect an output to an input";
        }
        emit statusMessage(why);
        return;
    }

    if (picked >= 0) {
        m_model->edit("Disconnect", [&] { m_model->removeConnection(picked); });
        return;
    }
    emit nodePopupRequested(scenePos, anchor->node()->nodeId(), anchor->index(), anchor->isOutput());
}

void NodeGraphScene::commitMoves()
{
    QHash<int, QPointF> positions;
    for (QGraphicsItem* it : selectedItems())
        if (it->type() == NodeItem::Type) positions.insert(static_cast<NodeItem*>(it)->nodeId(), it->pos());
    if (positions.isEmpty()) return;
    m_model->edit("Move nodes", [&] {
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) m_model->setNodePos(it.key(), it.value());
    });
}

int NodeGraphScene::addNodeAt(const QString& type, const QPointF& pos)
{
    int id = -1;
    m_model->edit("Add node", [&] { id = m_model->addNode(type, pos); });
    if (id < 0) {
        emit statusMessage("Only one Fragment Output node is allowed.");
        return -1;
    }
    selectNodes({id});
    return id;
}

void NodeGraphScene::createNodeAndConnect(const QString& type, const QPointF& pos, int fromNode, int fromPort,
                                          bool fromIsOutput)
{
    int id = -1;
    m_model->edit("Add node", [&] {
        id = m_model->addNode(type, pos);
        if (id < 0) return;
        const NodeDef* d = m_model->def(id);
        if (fromIsOutput) {
            for (int i = 0; i < d->inputs.size(); ++i) {
                if (m_model->addConnection(fromNode, fromPort, id, i) >= 0) break;
            }
        } else if (!d->outputs.isEmpty()) {
            m_model->addConnection(id, 0, fromNode, fromPort);
        }
    });
    if (id >= 0) selectNodes({id});
}

void NodeGraphScene::deleteSelection()
{
    QList<int> nodes = selectedNodeIds();
    QList<int> conns;
    for (QGraphicsItem* it : selectedItems())
        if (it->type() == ConnectionItem::Type) conns << static_cast<ConnectionItem*>(it)->connectionId();
    if (nodes.isEmpty() && conns.isEmpty()) return;
    m_model->edit(nodes.isEmpty() ? "Delete connection" : "Delete nodes", [&] {
        for (int c : conns) m_model->removeConnection(c);
        for (int n : nodes) m_model->removeNode(n);
    });
}

QString NodeGraphScene::clipboardJson() const
{
    const QList<int> ids = selectedNodeIds();
    if (ids.isEmpty()) return {};
    return QString::fromUtf8(QJsonDocument(m_model->toJson(&ids)).toJson(QJsonDocument::Compact));
}

void NodeGraphScene::copySelection()
{
    const QString json = clipboardJson();
    if (json.isEmpty()) return;
    QApplication::clipboard()->setText(json);
    m_pasteCount = 0;
}

void NodeGraphScene::cutSelection()
{
    copySelection();
    deleteSelection();
}

void NodeGraphScene::paste(const QPointF& scenePos)
{
    const QJsonDocument doc = QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8());
    if (!doc.isObject() || doc.object().value("format").toString() != "kesGraph") {
        emit statusMessage("Clipboard does not contain nodes.");
        return;
    }
    const QJsonObject obj = doc.object();
    qreal minX = 1e18, minY = 1e18;
    for (const QJsonValue& v : obj.value("nodes").toArray()) {
        minX = std::min(minX, v.toObject().value("x").toDouble());
        minY = std::min(minY, v.toObject().value("y").toDouble());
    }
    if (minX > 1e17) return;
    const QPointF offset = scenePos - QPointF(minX, minY) + QPointF(20, 20) * m_pasteCount;
    ++m_pasteCount;

    QList<int> ids;
    m_model->edit("Paste", [&] { ids = m_model->insertJson(obj, offset); });
    if (!ids.isEmpty()) selectNodes(ids);
}

void NodeGraphScene::duplicateSelection()
{
    const QList<int> ids = selectedNodeIds();
    if (ids.isEmpty()) return;
    const QJsonObject obj = m_model->toJson(&ids);
    QList<int> newIds;
    m_model->edit("Duplicate", [&] { newIds = m_model->insertJson(obj, QPointF(40, 40)); });
    if (!newIds.isEmpty()) selectNodes(newIds);
}

} // namespace sne

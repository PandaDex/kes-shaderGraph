#pragma once

#include "MaterialSettings.h"
#include "NodeLibrary.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QUndoStack>
#include <QVariantMap>
#include <QVector>
#include <functional>

namespace sne {

struct GraphNode {
    int id = 0;
    QString type;
    QPointF pos;
    QVariantMap params;
    QVariantMap inputDefaults;
};

struct GraphConnection {
    int id = 0;
    int fromNode = 0;
    int fromPort = 0;
    int toNode = 0;
    int toPort = 0;
};

class GraphModel : public QObject {
    Q_OBJECT
public:
    explicit GraphModel(QObject* parent = nullptr);

    QUndoStack* undoStack() { return &m_undo; }

    const GraphNode* node(int id) const;
    const NodeDef* def(int nodeId) const;
    QList<int> nodeIds() const;
    const QVector<GraphConnection>& connections() const { return m_connections; }
    const GraphConnection* connection(int id) const;
    const GraphConnection* inputConnection(int nodeId, int inputPort) const;
    QList<const GraphConnection*> outputConnections(int nodeId, int outputPort) const;
    int nodeCount() const { return m_nodes.size(); }
    bool hasOutputNode() const;
    int outputNodeId() const;

    const MaterialSettings& material() const { return m_material; }
    void setMaterial(const MaterialSettings& material);

    QVariant param(int nodeId, const QString& name) const;
    QVariantList inputDefault(int nodeId, int inputPort) const;

    bool canConnect(int fromNode, int fromPort, int toNode, int toPort, QString* why = nullptr) const;

    int addNode(const QString& type, const QPointF& pos, int forcedId = -1);
    void removeNode(int id);
    int addConnection(int fromNode, int fromPort, int toNode, int toPort);
    void removeConnection(int id);
    void setNodePos(int id, const QPointF& pos);
    void setParam(int id, const QString& name, const QVariant& value);
    void setInputDefault(int id, const QString& portName, const QVariantList& value);

    void edit(const QString& text, const std::function<void()>& fn, const QString& mergeKey = QString());

    QJsonObject toJson(const QList<int>* subset = nullptr) const;
    bool loadJson(const QJsonObject& obj, QStringList* warnings = nullptr);
    QList<int> insertJson(const QJsonObject& obj, const QPointF& offset, QStringList* warnings = nullptr);
    void clear();

signals:
    void nodeAdded(int id);
    void nodeRemoved(int id);
    void nodeChanged(int id);
    void nodePosChanged(int id);
    void connectionAdded(int id);
    void connectionRemoved(int id);
    void graphReset();
    void materialChanged();
    void changed();

private:
    bool parse(const QJsonObject& obj, bool remap, const QPointF& offset, QList<int>* newIds, QStringList* warnings);
    bool reaches(int fromNode, int targetNode) const;

    QHash<int, GraphNode> m_nodes;
    QVector<GraphConnection> m_connections;
    MaterialSettings m_material;
    int m_nextNodeId = 1;
    int m_nextConnId = 1;
    bool m_inEdit = false;
    QUndoStack m_undo;
    friend class SnapshotCommand;
};

} // namespace sne

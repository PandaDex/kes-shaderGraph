#pragma once

#include "GraphModel.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace sne {

enum class ShaderTarget { Standalone, KesEngine };

struct TypeInfo {
    QHash<int, PortType> generic;
    QHash<quint64, PortType> outputs;

    static quint64 key(int node, int port) { return (quint64(quint32(node)) << 16) | quint64(quint32(port)); }
    PortType outputType(int node, int port) const { return outputs.value(key(node, port), PortType::Float); }
    PortType genericType(int node) const { return generic.value(node, PortType::Float); }
};

TypeInfo inferTypes(const GraphModel& graph);

struct CodeGenResult {
    bool ok = false;
    QString code;
    QStringList errors;
    QStringList warnings;
    QHash<int, int> lineToNode;
};

class SlangCodeGen {
public:
    static CodeGenResult generate(const GraphModel& graph, const QString& graphName = QStringLiteral("NodeShader"),
                                  ShaderTarget target = ShaderTarget::Standalone);

    // Whether the output node changes anything over the plain .mat values.
    static bool hasSurfaceOverrides(const GraphModel& graph);
    static QString generatedHeader();

    static QString convertExpr(const QString& expr, PortType from, PortType to);
    static QString formatFloat(double v);
    static QString sanitizeIdentifier(const QString& s);
};

} // namespace sne

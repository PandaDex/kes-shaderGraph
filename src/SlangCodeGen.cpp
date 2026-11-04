#include "SlangCodeGen.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <functional>

namespace sne {

namespace {

PortType customOutputType(const QString& s)
{
    if (s == "float2") return PortType::Float2;
    if (s == "float3") return PortType::Float3;
    if (s == "float4") return PortType::Float4;
    return PortType::Float;
}

QString varName(int node, const QString& outName)
{
    return QString("n%1_%2").arg(node).arg(SlangCodeGen::sanitizeIdentifier(outName));
}

struct SurfaceField {
    const char* port;
    const char* assignment;
    PortType type;
};

const SurfaceField kSurfaceFields[] = {
    {"Color", "surface.baseColor = %1;", PortType::Float3},
    {"Emission", "surface.emissive = %1;", PortType::Float3},
    {"Alpha", "surface.alpha = %1;", PortType::Float},
    {"Metallic", "surface.metallic = %1;", PortType::Float},
    {"Roughness", "surface.roughness = %1;", PortType::Float},
    {"Normal", "surface.normal = KesTangentNormal(surfaceInput, %1);", PortType::Float3},
    {"Occlusion", "surface.occlusion = %1;", PortType::Float},
};

bool overridesField(const GraphModel& g, int outId, const NodeDef* d, const char* port)
{
    return g.inputConnection(outId, d->inputIndex(port)) || g.node(outId)->inputDefaults.contains(port);
}

QString literalFor(PortType target, const QVariantList& v)
{
    auto comp = [&](int i) { return SlangCodeGen::formatFloat(i < v.size() ? v[i].toDouble() : 0.0); };
    switch (target) {
    case PortType::Float:
    case PortType::Generic:
        return comp(0);
    case PortType::Float2:
    case PortType::Float3:
    case PortType::Float4: {
        const int n = dimension(target);
        if (v.size() <= 1) return QString("%1(%2)").arg(slangTypeName(target), comp(0));
        QStringList parts;
        for (int i = 0; i < n; ++i) parts << comp(i);
        return QString("%1(%2)").arg(slangTypeName(target), parts.join(", "));
    }
    case PortType::Int:
        return QString::number(v.isEmpty() ? 0 : v[0].toInt());
    case PortType::Bool:
        return (!v.isEmpty() && v[0].toDouble() != 0.0) ? "true" : "false";
    }
    return "0.0";
}

QString substitute(const QString& tmpl, const std::function<QString(const QString&)>& inLookup,
                   const std::function<QString(const QString&)>& paramLookup)
{
    static const QRegularExpression re(QStringLiteral(R"(\$\{(\w+)\}|\{(\w+)\})"));
    QString out;
    qsizetype last = 0;
    auto it = re.globalMatch(tmpl);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += tmpl.mid(last, m.capturedStart() - last);
        if (!m.captured(1).isEmpty()) out += paramLookup(m.captured(1));
        else out += inLookup(m.captured(2));
        last = m.capturedEnd();
    }
    out += tmpl.mid(last);
    return out;
}

} // namespace

QString SlangCodeGen::formatFloat(double v)
{
    QString s = QString::number(v, 'g', 9);
    if (!s.contains('.') && !s.contains('e') && !s.contains("inf") && !s.contains("nan")) s += ".0";
    return s;
}

QString SlangCodeGen::sanitizeIdentifier(const QString& in)
{
    QString s;
    for (QChar c : in) s += (c.isLetterOrNumber() || c == '_') ? c : QChar('_');
    if (s.isEmpty()) s = "unnamed";
    if (s[0].isDigit()) s.prepend('_');
    return s;
}

QString SlangCodeGen::convertExpr(const QString& e, PortType from, PortType to)
{
    if (from == to) return e;
    const bool fromF = isFloatFamily(from), toF = isFloatFamily(to);
    if (fromF && toF) {
        const int a = dimension(from), b = dimension(to);
        if (a == 1) return QString("%1(%2)").arg(slangTypeName(to), e);
        if (b == 1) return e + ".x";
        if (b < a) return e + "." + QString("xyzw").left(b);
        if (a == 2 && b == 3) return QString("float3(%1, 0.0)").arg(e);
        if (a == 2 && b == 4) return QString("float4(%1, 0.0, 1.0)").arg(e);
        if (a == 3 && b == 4) return QString("float4(%1, 1.0)").arg(e);
    }
    if (from == PortType::Int && toF) {
        const QString f = QString("float(%1)").arg(e);
        return dimension(to) == 1 ? f : QString("%1(%2)").arg(slangTypeName(to), f);
    }
    if (from == PortType::Bool && toF) {
        const QString f = QString("(%1 ? 1.0 : 0.0)").arg(e);
        return dimension(to) == 1 ? f : QString("%1(%2)").arg(slangTypeName(to), f);
    }
    if (fromF && to == PortType::Int) return QString("int(%1)").arg(dimension(from) == 1 ? e : e + ".x");
    if (fromF && to == PortType::Bool) return QString("(%1 != 0.0)").arg(dimension(from) == 1 ? e : e + ".x");
    if (from == PortType::Int && to == PortType::Bool) return QString("(%1 != 0)").arg(e);
    if (from == PortType::Bool && to == PortType::Int) return QString("(%1 ? 1 : 0)").arg(e);
    return e;
}

TypeInfo inferTypes(const GraphModel& g)
{
    TypeInfo ti;
    QSet<int> visiting;
    std::function<PortType(int)> resolve;
    std::function<PortType(int, int)> outType;

    outType = [&](int nodeId, int port) -> PortType {
        const quint64 k = TypeInfo::key(nodeId, port);
        auto it = ti.outputs.constFind(k);
        if (it != ti.outputs.constEnd()) return it.value();
        const NodeDef* d = g.def(nodeId);
        if (!d) return PortType::Float;
        PortType t;
        if (d->role == NodeRole::CustomExpression) {
            t = customOutputType(g.param(nodeId, "Output").toString());
        } else {
            const PortType declared = d->outputs.value(port).type;
            t = declared == PortType::Generic ? resolve(nodeId) : declared;
        }
        ti.outputs.insert(k, t);
        return t;
    };

    resolve = [&](int nodeId) -> PortType {
        auto it = ti.generic.constFind(nodeId);
        if (it != ti.generic.constEnd()) return it.value();
        const NodeDef* d = g.def(nodeId);
        if (!d || visiting.contains(nodeId)) return PortType::Float;
        visiting.insert(nodeId);
        int dim = 0;
        for (int i = 0; i < d->inputs.size(); ++i) {
            if (d->inputs[i].type != PortType::Generic) continue;
            if (const GraphConnection* c = g.inputConnection(nodeId, i))
                dim = std::max(dim, dimension(outType(c->fromNode, c->fromPort)));
        }
        visiting.remove(nodeId);
        dim = std::max({dim, d->genericMinDim, 1});
        const PortType t = floatTypeOfDim(dim);
        ti.generic.insert(nodeId, t);
        return t;
    };

    for (int id : g.nodeIds()) {
        const NodeDef* d = g.def(id);
        if (!d) continue;
        resolve(id);
        for (int p = 0; p < d->outputs.size(); ++p) outType(id, p);
    }
    return ti;
}

QString SlangCodeGen::generatedHeader()
{
    return QStringLiteral("// Generated by kes-ShaderGraph.");
}

bool SlangCodeGen::hasSurfaceOverrides(const GraphModel& g)
{
    const int outId = g.outputNodeId();
    const NodeDef* d = g.def(outId);
    if (!d) return false;
    for (const SurfaceField& f : kSurfaceFields)
        if (overridesField(g, outId, d, f.port)) return true;
    return false;
}

CodeGenResult SlangCodeGen::generate(const GraphModel& g, const QString& graphName, ShaderTarget target)
{
    CodeGenResult res;
    const bool engine = target == ShaderTarget::KesEngine;

    const int outId = g.outputNodeId();
    if (outId < 0) {
        res.errors << "The graph has no Fragment Output node.";
        return res;
    }

    const TypeInfo ti = inferTypes(g);

    QVector<int> order;
    QSet<int> visited;
    std::function<void(int)> visit = [&](int id) {
        if (visited.contains(id)) return;
        visited.insert(id);
        const NodeDef* d = g.def(id);
        if (!d) return;
        for (int i = 0; i < d->inputs.size(); ++i)
            if (const GraphConnection* c = g.inputConnection(id, i)) visit(c->fromNode);
        order.append(id);
    };
    visit(outId);

    QSet<quint64> usedOutputs;
    for (const GraphConnection& c : g.connections())
        if (visited.contains(c.toNode)) usedOutputs.insert(TypeInfo::key(c.fromNode, c.fromPort));

    QStringList body;
    QVector<int> bodyNode;
    auto emitLine = [&](const QString& line, int nodeId) {
        body << QStringLiteral("    ") + line;
        bodyNode << nodeId;
    };

    QSet<QString> usedHelpers;
    QVector<QPair<QString, PortType>> uniforms;
    QStringList textures;
    QHash<QString, QString> propertyValues;

    // Node expressions are written against the standalone declarations; the
    // engine has its own names, a fixed texture set and no custom constants.
    auto retarget = [&](QString expr, int id, const NodeDef* d) {
        if (!engine) return expr;
        expr.replace(QStringLiteral("gScene.time"), QStringLiteral("KesTime()"));
        expr.replace(QStringLiteral("gScene.cameraPos"), QStringLiteral("uCameraPosition.xyz"));
        expr.replace(QStringLiteral("gMaterial."), QStringLiteral("kProp_"));
        if (!d->textureNameParam.isEmpty()) {
            static const QRegularExpression sample(QStringLiteral(R"(tex_\w+\.Sample\(gSampler,\s*)"));
            const int slot = materialSlotFromLabel(g.param(id, "Slot").toString());
            expr.replace(sample, QString::fromLatin1(materialSlotInfo(slot).shaderVar) + QStringLiteral(".Sample("));
        }
        return expr;
    };

    auto inputExpr = [&](int id, const NodeDef* d, int i, PortType target) -> QString {
        const PortDef& port = d->inputs[i];
        if (const GraphConnection* c = g.inputConnection(id, i)) {
            const NodeDef* sd = g.def(c->fromNode);
            const PortType src = ti.outputType(c->fromNode, c->fromPort);
            const QString v = varName(c->fromNode, sd->outputs[c->fromPort].name);
            return convertExpr(v, src, target);
        }
        if (!port.defaultExpr.isEmpty()) return port.defaultExpr;
        return literalFor(target, g.inputDefault(id, i));
    };

    auto paramText = [&](int id, const NodeDef* d, const QString& name) -> QString {
        const ParamDef* pd = d->paramDef(name);
        if (!pd) return name;
        const QVariant v = g.param(id, name);
        switch (pd->kind) {
        case ParamKind::Float: return formatFloat(v.toDouble());
        case ParamKind::Int: return QString::number(v.toInt());
        case ParamKind::Bool: return v.toBool() ? "true" : "false";
        case ParamKind::Identifier: return sanitizeIdentifier(v.toString());
        case ParamKind::Color: {
            const QVariantList l = v.toList();
            QStringList parts;
            for (int i = 0; i < 4; ++i) parts << formatFloat(i < l.size() ? l[i].toDouble() : (i == 3 ? 1.0 : 0.0));
            return parts.join(", ");
        }
        default: return v.toString();
        }
    };

    auto propertyValue = [&](int id, const NodeDef* d) -> QString {
        switch (d->uniformType) {
        case PortType::Float3:
            return QString("float3(%1, %2, %3)").arg(paramText(id, d, "X"), paramText(id, d, "Y"), paramText(id, d, "Z"));
        case PortType::Float4:
            return QString("float4(%1)").arg(paramText(id, d, "Default"));
        default:
            return paramText(id, d, "Default");
        }
    };

    for (int id : order) {
        const NodeDef* d = g.def(id);
        if (!d || d->role == NodeRole::FragmentOutput) continue;

        if (d->engineOnly && !engine)
            res.errors << QString("%1 (#%2) is only available on the KES target.").arg(d->title).arg(id);

        for (const QString& h : d->helpers) usedHelpers.insert(h);

        if (!d->uniformNameParam.isEmpty()) {
            const QString name = sanitizeIdentifier(g.param(id, d->uniformNameParam).toString());
            bool found = false;
            for (const auto& u : uniforms) {
                if (u.first == name) {
                    found = true;
                    if (u.second != d->uniformType)
                        res.errors << QString("Property '%1' is used with conflicting types.").arg(name);
                }
            }
            if (!found) uniforms.append({name, d->uniformType});
            if (engine) {
                const QString value = propertyValue(id, d);
                if (found && propertyValues.value(name) != value)
                    res.warnings << QString("Property '%1' has different defaults; using the first.").arg(name);
                if (!found) propertyValues.insert(name, value);
            }
        }
        if (!d->textureNameParam.isEmpty()) {
            const QString name = sanitizeIdentifier(g.param(id, d->textureNameParam).toString());
            if (!textures.contains(name)) textures << name;
            if (engine) {
                const int slot = materialSlotFromLabel(g.param(id, "Slot").toString());
                if (g.material().textures[slot].isEmpty())
                    res.warnings << QString("Sample Texture 2D (#%1): the %2 slot has no texture, the engine binds white.")
                                        .arg(id).arg(QString::fromLatin1(materialSlotInfo(slot).label));
            }
        }

        emitLine(QString("// %1 (#%2)").arg(d->title).arg(id), id);

        if (d->role == NodeRole::CustomExpression) {
            const PortType outT = ti.outputType(id, 0);
            const QString var = varName(id, d->outputs[0].name);
            QString expr = g.param(id, "Expression").toString().trimmed();
            if (expr.isEmpty()) {
                res.warnings << QString("Custom Expression #%1 is empty; using 0.").arg(id);
                expr = literalFor(outT, {0.0});
            }
            emitLine(QString("%1 %2;").arg(slangTypeName(outT), var), id);
            emitLine("{", id);
            const char* names[4] = {"A", "B", "C", "D"};
            for (int i = 0; i < 4 && i < d->inputs.size(); ++i) {
                PortType t = PortType::Float;
                QString init;
                if (const GraphConnection* c = g.inputConnection(id, i)) {
                    t = ti.outputType(c->fromNode, c->fromPort);
                    init = varName(c->fromNode, g.def(c->fromNode)->outputs[c->fromPort].name);
                } else {
                    init = literalFor(PortType::Float, g.inputDefault(id, i));
                }
                emitLine(QString("    %1 %2 = %3;").arg(slangTypeName(t), names[i], init), id);
            }
            emitLine(QString("    %1 = %2;").arg(var, retarget(expr, id, d)), id);
            emitLine("}", id);
            continue;
        }

        const PortType resolved = ti.genericType(id);
        QStringList inExprs;
        for (int i = 0; i < d->inputs.size(); ++i) {
            const PortType target = d->inputs[i].type == PortType::Generic ? resolved : d->inputs[i].type;
            inExprs << inputExpr(id, d, i, target);
        }
        auto lookupIn = [&](const QString& name) {
            const int idx = d->inputIndex(name);
            return idx >= 0 ? inExprs[idx] : name;
        };
        auto lookupParam = [&](const QString& name) { return paramText(id, d, name); };

        for (int o = 0; o < d->outputs.size(); ++o) {
            if (!usedOutputs.contains(TypeInfo::key(id, o))) continue;
            const QString expr = retarget(substitute(d->outputs[o].expr, lookupIn, lookupParam), id, d);
            emitLine(QString("%1 %2 = %3;").arg(slangTypeName(ti.outputType(id, o)), varName(id, d->outputs[o].name), expr), id);
        }
    }

    {
        const NodeDef* d = g.def(outId);
        emitLine(QString("// %1 (#%2)").arg(d->title).arg(outId), outId);
        if (engine) {
            for (const SurfaceField& f : kSurfaceFields) {
                if (overridesField(g, outId, d, f.port))
                    emitLine(QString(f.assignment).arg(inputExpr(outId, d, d->inputIndex(f.port), f.type)), outId);
            }
            emitLine("return KesShadeMaterial(surfaceInput, surface);", outId);
        } else {
            for (const char* port : {"Metallic", "Roughness", "Normal", "Occlusion"}) {
                if (g.inputConnection(outId, d->inputIndex(port)))
                    res.warnings << QString("%1 is only used by the KES target.").arg(port);
            }
            const QString color = inputExpr(outId, d, d->inputIndex("Color"), PortType::Float3);
            const QString emission = inputExpr(outId, d, d->inputIndex("Emission"), PortType::Float3);
            const QString alpha = inputExpr(outId, d, d->inputIndex("Alpha"), PortType::Float);
            emitLine(QString("return float4(%1 + %2, %3);").arg(color, emission, alpha), outId);
        }
    }

    QStringList needed;
    {
        QSet<QString> keys = usedHelpers;
        bool grew = true;
        while (grew) {
            grew = false;
            for (const QString& k : QStringList(keys.begin(), keys.end())) {
                if (const HelperDef* h = NodeLibrary::instance().helper(k)) {
                    for (const QString& dep : h->deps) {
                        if (!keys.contains(dep)) { keys.insert(dep); grew = true; }
                    }
                }
            }
        }
        for (const HelperDef& h : NodeLibrary::instance().helpers()) {
            if (keys.contains(h.key)) needed << h.code.split('\n') << "";
        }
    }

    QStringList head;
    head << generatedHeader() + " Edit the graph, not this file."
         << QString("// Graph: %1  |  %2 node(s) in use").arg(graphName).arg(order.size())
         << "";

    if (engine) {
        head << "#include \"kes_common.slang\""
             << "";
        if (!uniforms.isEmpty()) {
            res.warnings << "Properties are baked into the KES shader as constants: the engine has no per-material custom values.";
            for (const auto& u : uniforms)
                head << QString("static const %1 kProp_%2 = %3;").arg(slangTypeName(u.second), u.first, propertyValues.value(u.first));
            head << "";
        }
        head << needed
             << "struct VSOutput"
             << "{"
             << "    float4 position : SV_Position;"
             << "    float3 worldPos : TEXCOORD1;"
             << "    float3 normal   : NORMAL;"
             << "    float4 color    : COLOR0;"
             << "    float2 uv       : TEXCOORD0;"
             << "};"
             << ""
             << "[shader(\"vertex\")]"
             << "VSOutput vsMain(MeshVertexInput vertex)"
             << "{"
             << "    MeshVertexResult v = KesMeshVertex(vertex);"
             << "    VSOutput output;"
             << "    output.position = v.clipPosition;"
             << "    output.worldPos = v.worldPosition;"
             << "    output.normal = v.worldNormal;"
             << "    output.color = v.color;"
             << "    output.uv = v.uv;"
             << "    return output;"
             << "}"
             << ""
             << "[shader(\"fragment\")]"
             << "float4 psMain(VSOutput input, bool isFrontFace : SV_IsFrontFace) : SV_Target"
             << "{"
             << "    SurfaceInput surfaceInput;"
             << "    surfaceInput.worldPosition = input.worldPos;"
             << "    surfaceInput.worldNormal = input.normal;"
             << "    surfaceInput.vertexColor = input.color;"
             << "    surfaceInput.uv = input.uv;"
             << "    surfaceInput.frontFace = isFrontFace;"
             << "    KesSurface surface = KesMaterialSurface(surfaceInput);";
    } else {
        head << "struct SceneParams"
             << "{"
             << "    float4x4 viewProj;"
             << "    float4x4 model;"
             << "    float3   cameraPos;"
             << "    float    time;"
             << "};"
             << "ConstantBuffer<SceneParams> gScene;"
             << "";

        if (!uniforms.isEmpty()) {
            head << "struct MaterialParams"
                 << "{";
            for (const auto& u : uniforms) head << QString("    %1 %2;").arg(slangTypeName(u.second), u.first);
            head << "};"
                 << "ConstantBuffer<MaterialParams> gMaterial;"
                 << "";
        }
        head << "SamplerState gSampler;";
        for (const QString& t : textures) head << QString("Texture2D tex_%1;").arg(t);
        head << "";

        head << needed
             << "struct VSInput"
             << "{"
             << "    float3 position : POSITION;"
             << "    float3 normal   : NORMAL;"
             << "    float2 uv       : TEXCOORD0;"
             << "};"
             << ""
             << "struct VSOutput"
             << "{"
             << "    float4 position : SV_Position;"
             << "    float3 worldPos : TEXCOORD1;"
             << "    float3 normal   : TEXCOORD2;"
             << "    float2 uv       : TEXCOORD0;"
             << "};"
             << ""
             << "[shader(\"vertex\")]"
             << "VSOutput vertexMain(VSInput input)"
             << "{"
             << "    VSOutput output;"
             << "    float4 world = mul(gScene.model, float4(input.position, 1.0));"
             << "    output.worldPos = world.xyz;"
             << "    output.normal = mul((float3x3)gScene.model, input.normal);"
             << "    output.uv = input.uv;"
             << "    output.position = mul(gScene.viewProj, world);"
             << "    return output;"
             << "}"
             << ""
             << "[shader(\"fragment\")]"
             << "float4 fragmentMain(VSOutput input) : SV_Target"
             << "{";
    }

    QStringList foot;
    foot << "}";

    QStringList all = head;
    const int bodyStart = all.size();
    all << body << foot;
    for (int i = 0; i < body.size(); ++i) res.lineToNode.insert(bodyStart + i + 1, bodyNode[i]);

    res.code = all.join('\n') + '\n';
    res.ok = res.errors.isEmpty();
    return res;
}

} // namespace sne

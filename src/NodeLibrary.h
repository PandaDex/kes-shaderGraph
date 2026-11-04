#pragma once

#include "Types.h"

#include <QHash>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace sne {

enum class ParamKind { Float, Int, Bool, Enum, Identifier, Code, Color };

struct ParamDef {
    QString name;
    ParamKind kind = ParamKind::Float;
    QVariant defaultValue;
    QStringList options;
    double minValue = -1e6;
    double maxValue = 1e6;
    double step = 0.05;
};

struct PortDef {
    QString name;
    PortType type = PortType::Float;
    QVariantList defaultValue;
    QString defaultExpr;
};

struct OutputDef {
    QString name;
    PortType type = PortType::Float;
    QString expr;
};

enum class NodeRole { Normal, FragmentOutput, CustomExpression };

struct NodeDef {
    QString type;
    QString title;
    QString category;
    QString description;
    QVector<PortDef> inputs;
    QVector<OutputDef> outputs;
    QVector<ParamDef> params;
    QStringList helpers;
    NodeRole role = NodeRole::Normal;
    QString uniformNameParam;
    PortType uniformType = PortType::Float;
    QString textureNameParam;
    int genericMinDim = 1;
    bool engineOnly = false;

    int inputIndex(const QString& n) const
    {
        for (int i = 0; i < inputs.size(); ++i) if (inputs[i].name == n) return i;
        return -1;
    }
    int outputIndex(const QString& n) const
    {
        for (int i = 0; i < outputs.size(); ++i) if (outputs[i].name == n) return i;
        return -1;
    }
    const ParamDef* paramDef(const QString& n) const
    {
        for (const ParamDef& p : params) if (p.name == n) return &p;
        return nullptr;
    }

    NodeDef& in(const QString& n, PortType t, QVariantList def = {}, const QString& defExpr = {})
    {
        inputs.append(PortDef{n, t, def, defExpr});
        return *this;
    }
    NodeDef& out(const QString& n, PortType t, const QString& expr)
    {
        outputs.append(OutputDef{n, t, expr});
        return *this;
    }
    NodeDef& pFloat(const QString& n, double def, double mn = -1e6, double mx = 1e6, double step = 0.05)
    {
        ParamDef p; p.name = n; p.kind = ParamKind::Float; p.defaultValue = def;
        p.minValue = mn; p.maxValue = mx; p.step = step;
        params.append(p);
        return *this;
    }
    NodeDef& pInt(const QString& n, int def, int mn = -1000000, int mx = 1000000)
    {
        ParamDef p; p.name = n; p.kind = ParamKind::Int; p.defaultValue = def;
        p.minValue = mn; p.maxValue = mx;
        params.append(p);
        return *this;
    }
    NodeDef& pBool(const QString& n, bool def)
    {
        ParamDef p; p.name = n; p.kind = ParamKind::Bool; p.defaultValue = def;
        params.append(p);
        return *this;
    }
    NodeDef& pEnum(const QString& n, const QStringList& options, const QString& def)
    {
        ParamDef p; p.name = n; p.kind = ParamKind::Enum; p.defaultValue = def; p.options = options;
        params.append(p);
        return *this;
    }
    NodeDef& pIdent(const QString& n, const QString& def)
    {
        ParamDef p; p.name = n; p.kind = ParamKind::Identifier; p.defaultValue = def;
        params.append(p);
        return *this;
    }
    NodeDef& pCode(const QString& n, const QString& def)
    {
        ParamDef p; p.name = n; p.kind = ParamKind::Code; p.defaultValue = def;
        params.append(p);
        return *this;
    }
    NodeDef& pColor(const QString& n, double r, double g, double b, double a)
    {
        ParamDef p; p.name = n; p.kind = ParamKind::Color;
        p.defaultValue = QVariantList{r, g, b, a};
        params.append(p);
        return *this;
    }
    NodeDef& needs(const QStringList& h) { helpers = h; return *this; }
    NodeDef& minDim(int d) { genericMinDim = d; return *this; }
    NodeDef& uniform(const QString& nameParam, PortType t) { uniformNameParam = nameParam; uniformType = t; return *this; }
    NodeDef& texture(const QString& nameParam) { textureNameParam = nameParam; return *this; }
    NodeDef& withRole(NodeRole r) { role = r; return *this; }
    NodeDef& forEngineOnly() { engineOnly = true; return *this; }
};

struct HelperDef {
    QString key;
    QStringList deps;
    QString code;
};

class NodeLibrary {
public:
    static const NodeLibrary& instance();

    const NodeDef* find(const QString& type) const;
    const QVector<NodeDef>& all() const { return m_defs; }
    QStringList categories() const;
    const QVector<HelperDef>& helpers() const { return m_helpers; }
    const HelperDef* helper(const QString& key) const;

private:
    NodeLibrary();
    QVector<NodeDef> m_defs;
    QHash<QString, int> m_index;
    QVector<HelperDef> m_helpers;
};

} // namespace sne

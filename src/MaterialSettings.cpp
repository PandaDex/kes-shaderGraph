#include "MaterialSettings.h"

#include <QJsonArray>

namespace sne {

namespace {

const MaterialSlotInfo kSlots[MaterialSlotCount] = {
    {"Base Color", "base", "BaseTexture", "uBaseTexture", true},
    {"Normal", "normal", "NormalTexture", "uNormalTexture", false},
    {"Metallic Roughness", "metallicRoughness", "MetallicRoughnessTexture", "uMetallicRoughnessTexture", false},
    {"Occlusion", "occlusion", "OcclusionTexture", "uOcclusionTexture", false},
    {"Emissive", "emissive", "EmissiveTexture", "uEmissiveTexture", true},
};

template <size_t N>
QJsonArray toArray(const std::array<double, N>& v)
{
    QJsonArray a;
    for (double d : v) a.append(d);
    return a;
}

template <size_t N>
void readArray(const QJsonValue& value, std::array<double, N>& out)
{
    const QJsonArray a = value.toArray();
    for (size_t i = 0; i < N && int(i) < a.size(); ++i) out[i] = a[int(i)].toDouble(out[i]);
}

QString number(double v) { return QString::number(v, 'g', 7); }
QString boolText(bool v) { return v ? QStringLiteral("true") : QStringLiteral("false"); }
QString quoted(QString s) { return '"' + s.replace('\\', '/').remove('"') + '"'; }

} // namespace

const MaterialSlotInfo& materialSlotInfo(int slot) { return kSlots[slot]; }

QStringList materialSlotLabels()
{
    QStringList labels;
    for (const MaterialSlotInfo& s : kSlots) labels << QString::fromLatin1(s.label);
    return labels;
}

int materialSlotFromLabel(const QString& label)
{
    for (int i = 0; i < MaterialSlotCount; ++i)
        if (label == QLatin1String(kSlots[i].label)) return i;
    return SlotBaseColor;
}

QJsonObject MaterialSettings::toJson() const
{
    QJsonObject o;
    o["baseColor"] = toArray(baseColor);
    o["emissive"] = toArray(emissive);
    o["emissiveIntensity"] = emissiveIntensity;
    o["metallic"] = metallic;
    o["roughness"] = roughness;
    o["normalScale"] = normalScale;
    o["occlusionStrength"] = occlusionStrength;
    o["alphaCutoff"] = alphaCutoff;
    o["lit"] = lit;
    o["transparent"] = transparent;
    o["doubleSided"] = doubleSided;
    o["castShadows"] = castShadows;
    QJsonObject tex;
    for (int i = 0; i < MaterialSlotCount; ++i)
        if (!textures[i].isEmpty()) tex[kSlots[i].jsonKey] = textures[i];
    o["textures"] = tex;
    return o;
}

MaterialSettings MaterialSettings::fromJson(const QJsonObject& o)
{
    MaterialSettings m;
    readArray(o.value("baseColor"), m.baseColor);
    readArray(o.value("emissive"), m.emissive);
    m.emissiveIntensity = o.value("emissiveIntensity").toDouble(m.emissiveIntensity);
    m.metallic = o.value("metallic").toDouble(m.metallic);
    m.roughness = o.value("roughness").toDouble(m.roughness);
    m.normalScale = o.value("normalScale").toDouble(m.normalScale);
    m.occlusionStrength = o.value("occlusionStrength").toDouble(m.occlusionStrength);
    m.alphaCutoff = o.value("alphaCutoff").toDouble(m.alphaCutoff);
    m.lit = o.value("lit").toBool(m.lit);
    m.transparent = o.value("transparent").toBool(m.transparent);
    m.doubleSided = o.value("doubleSided").toBool(m.doubleSided);
    m.castShadows = o.value("castShadows").toBool(m.castShadows);
    const QJsonObject tex = o.value("textures").toObject();
    for (int i = 0; i < MaterialSlotCount; ++i) m.textures[i] = tex.value(kSlots[i].jsonKey).toString();
    return m;
}

QString MaterialSettings::toMatFile(const QString& shaderPath) const
{
    QStringList l;
    l << "Material {";
    if (!shaderPath.isEmpty()) l << "    Shader = " + quoted(shaderPath);
    if (!textures[SlotBaseColor].isEmpty()) l << "    BaseTexture = " + quoted(textures[SlotBaseColor]);
    l << QString("    BaseColor = \"%1,%2,%3,%4\"")
             .arg(number(baseColor[0]), number(baseColor[1]), number(baseColor[2]), number(baseColor[3]));
    l << QString("    Emissive = \"%1,%2,%3\"").arg(number(emissive[0]), number(emissive[1]), number(emissive[2]));
    l << "    Metallic = " + number(metallic);
    l << "    Roughness = " + number(roughness);
    for (int i = SlotNormal; i < MaterialSlotCount; ++i)
        if (!textures[i].isEmpty()) l << QString("    %1 = %2").arg(kSlots[i].matKey, quoted(textures[i]));
    if (normalScale != 1.0) l << "    NormalScale = " + number(normalScale);
    if (occlusionStrength != 1.0) l << "    OcclusionStrength = " + number(occlusionStrength);
    if (emissiveIntensity != 1.0) l << "    EmissiveIntensity = " + number(emissiveIntensity);
    l << "    AlphaCutoff = " + number(alphaCutoff);
    l << "    Lit = " + boolText(lit);
    l << "    Transparent = " + boolText(transparent);
    l << "    DoubleSided = " + boolText(doubleSided);
    l << "    CastShadows = " + boolText(castShadows);
    l << "}";
    return l.join('\n') + '\n';
}

} // namespace sne

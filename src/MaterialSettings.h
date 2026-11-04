#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <array>

namespace sne {

// Order matches the engine's material texture bindings (t0-t4).
enum MaterialSlot { SlotBaseColor, SlotNormal, SlotMetallicRoughness, SlotOcclusion, SlotEmissive, MaterialSlotCount };

struct MaterialSlotInfo {
    const char* label;
    const char* jsonKey;
    const char* matKey;
    const char* shaderVar;
    bool srgb;
};

const MaterialSlotInfo& materialSlotInfo(int slot);
QStringList materialSlotLabels();
int materialSlotFromLabel(const QString& label);

// What the engine's .mat file holds (engine/material.h).
struct MaterialSettings {
    std::array<double, 4> baseColor{1.0, 1.0, 1.0, 1.0};
    std::array<double, 3> emissive{0.0, 0.0, 0.0};
    double emissiveIntensity = 1.0;
    double metallic = 0.0;
    double roughness = 0.8;
    double normalScale = 1.0;
    double occlusionStrength = 1.0;
    double alphaCutoff = 0.0;
    bool lit = true;
    bool transparent = false;
    bool doubleSided = false;
    bool castShadows = true;
    std::array<QString, MaterialSlotCount> textures;

    QJsonObject toJson() const;
    static MaterialSettings fromJson(const QJsonObject& obj);

    // shaderPath is content-relative; empty means the engine's default PBR program.
    QString toMatFile(const QString& shaderPath) const;
};

} // namespace sne

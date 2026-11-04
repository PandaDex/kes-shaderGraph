#pragma once

#include <QString>
#include <QStringList>

namespace sne {

class GraphModel;

// One engine material: <content>/.../name.mat, plus shaders/name.shader and
// shaders/name.slang when the graph drives the surface. A graph with nothing
// wired into the output is a plain material on the engine's PBR program.
struct MaterialFiles {
    QString matPath, matText;
    QString shaderPath, shaderText;
    QString slangPath, slangText;

    bool hasShader() const { return !slangPath.isEmpty(); }
};

bool buildMaterialFiles(const GraphModel& graph, const QString& matPath, const QString& contentRoot,
                        MaterialFiles& out, QString* error);

// Existing shader files that would be overwritten but weren't written by this tool.
QStringList handWrittenFiles(const MaterialFiles& files);

bool writeMaterialFiles(const MaterialFiles& files, QString* error);

} // namespace sne

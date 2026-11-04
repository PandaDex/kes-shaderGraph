#include "MaterialExport.h"

#include "GraphModel.h"
#include "KesProject.h"
#include "SlangCodeGen.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace sne {

namespace {

bool writeText(const QString& path, const QString& text, QString* error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error = QString("Could not create the folder for %1.").arg(path);
        return false;
    }
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(text.toUtf8());
        if (f.commit()) return true;
    }
    if (error) *error = QString("Could not write %1:\n%2").arg(path, f.errorString());
    return false;
}

QString readText(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

} // namespace

bool buildMaterialFiles(const GraphModel& graph, const QString& matPath, const QString& contentRoot,
                        MaterialFiles& out, QString* error)
{
    auto fail = [&](const QString& message) {
        if (error) *error = message;
        return false;
    };

    if (contentRelativePath(contentRoot, matPath).isEmpty())
        return fail(QString("%1 is outside the content folder (%2), so the engine could not load it.")
                        .arg(matPath, contentRoot.isEmpty() ? QStringLiteral("not set") : contentRoot));

    const QString name = SlangCodeGen::sanitizeIdentifier(QFileInfo(matPath).completeBaseName());
    out = {};
    out.matPath = matPath;

    QString shaderAsset;
    if (SlangCodeGen::hasSurfaceOverrides(graph)) {
        const CodeGenResult gen = SlangCodeGen::generate(graph, name, ShaderTarget::KesEngine);
        if (!gen.ok) return fail(gen.errors.join('\n'));

        const QDir root(contentRoot);
        shaderAsset = QString("shaders/%1.shader").arg(name);
        const QString source = QString("shaders/%1.slang").arg(name);
        out.shaderPath = root.absoluteFilePath(shaderAsset);
        out.slangPath = root.absoluteFilePath(source);
        out.slangText = gen.code;
        out.shaderText = QString("Shader {\n    Name = \"%1\"\n    Source = \"%2\"\n}\n").arg(name, source);
    }
    out.matText = graph.material().toMatFile(shaderAsset);
    return true;
}

QStringList handWrittenFiles(const MaterialFiles& files)
{
    QStringList result;
    if (!files.hasShader()) return result;
    if (QFileInfo::exists(files.slangPath) && !readText(files.slangPath).startsWith(SlangCodeGen::generatedHeader()))
        result << files.slangPath;
    if (QFileInfo::exists(files.shaderPath) && readText(files.shaderPath) != files.shaderText) result << files.shaderPath;
    return result;
}

bool writeMaterialFiles(const MaterialFiles& files, QString* error)
{
    if (files.hasShader()) {
        if (!writeText(files.slangPath, files.slangText, error)) return false;
        if (!writeText(files.shaderPath, files.shaderText, error)) return false;
    }
    return writeText(files.matPath, files.matText, error);
}

} // namespace sne

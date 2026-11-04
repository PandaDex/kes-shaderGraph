#include "KesProject.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace sne {

QString contentRootFromProject(const QString& kprojectPath)
{
    QFile f(kprojectPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    static const QRegularExpression re(QStringLiteral(R"re(AssetRoot\s*=\s*"?([^"\r\n]+?)"?\s*$)re"),
                                       QRegularExpression::MultilineOption);
    const QRegularExpressionMatch m = re.match(QString::fromUtf8(f.readAll()));
    if (!m.hasMatch()) return {};
    return QDir::cleanPath(QFileInfo(kprojectPath).absoluteDir().absoluteFilePath(m.captured(1)));
}

QString findContentRoot(const QString& startPath)
{
    const QFileInfo start(startPath);
    QDir dir(start.isDir() ? start.absoluteFilePath() : start.absolutePath());
    do {
        const QStringList projects = dir.entryList({"*.kproject"}, QDir::Files);
        if (!projects.isEmpty()) {
            const QString root = contentRootFromProject(dir.absoluteFilePath(projects.first()));
            if (!root.isEmpty()) return root;
        }
        if (dir.dirName() == QLatin1String("content")) return dir.absolutePath();
    } while (dir.cdUp());
    return {};
}

QString contentRelativePath(const QString& contentRoot, const QString& absolutePath)
{
    if (contentRoot.isEmpty()) return {};
    const QString rel = QDir(contentRoot).relativeFilePath(absolutePath);
    if (rel.startsWith(QLatin1String("..")) || QDir::isAbsolutePath(rel)) return {};
    return rel;
}

QString findSlangc(const QStringList& startDirs)
{
#ifdef Q_OS_WIN
    const QString bundled = QStringLiteral("external/slang/win64/bin/slangc.exe");
    const QString sdkBin = QStringLiteral("Bin/slangc.exe");
#else
    const QString bundled = QStringLiteral("external/slang/linux64/bin/slangc");
    const QString sdkBin = QStringLiteral("bin/slangc");
#endif
    for (const QString& start : startDirs) {
        if (start.isEmpty()) continue;
        QDir dir(start);
        do {
            if (QFileInfo::exists(dir.absoluteFilePath(bundled))) return dir.absoluteFilePath(bundled);
        } while (dir.cdUp());
    }
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("slangc"));
    if (!onPath.isEmpty()) return onPath;
    const QString sdk = qEnvironmentVariable("VULKAN_SDK");
    if (!sdk.isEmpty() && QFileInfo::exists(QDir(sdk).absoluteFilePath(sdkBin))) return QDir(sdk).absoluteFilePath(sdkBin);
    return {};
}

} // namespace sne

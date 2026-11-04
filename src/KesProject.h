#pragma once

#include <QString>
#include <QStringList>

namespace sne {

// Absolute content directory named by a .kproject (its AssetRoot).
QString contentRootFromProject(const QString& kprojectPath);

// Walks up from a file or directory to the nearest .kproject or "content" folder.
QString findContentRoot(const QString& startPath);

// Forward-slash path relative to the content root, empty when outside it.
QString contentRelativePath(const QString& contentRoot, const QString& absolutePath);

// Searches each start directory and its parents for the engine's bundled
// slangc, then PATH and the Vulkan SDK.
QString findSlangc(const QStringList& startDirs);

} // namespace sne

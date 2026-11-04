#include "PreviewCompiler.h"

#include <QFile>
#include <QProcess>

namespace sne {

namespace {

QString readText(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

} // namespace

PreviewCompiler::PreviewCompiler(QObject* parent) : QObject(parent)
{
    // Copies of resources are read-only, which would stop the temp dir being removed.
    const QString common = m_dir.filePath(QStringLiteral("kes_common.slang"));
    QFile::copy(QStringLiteral(":/preview/kes_common.slang"), common);
    QFile::setPermissions(common, QFile::ReadOwner | QFile::WriteOwner);
}

void PreviewCompiler::compile(const QString& source)
{
    m_pending = source;
    m_hasPending = true;
    if (m_running == 0) start();
}

void PreviewCompiler::start()
{
    m_hasPending = false;
    m_log.clear();

    if (m_slangc.isEmpty()) {
        emit failed(QStringLiteral("slangc not found. Use File > Locate slangc."));
        return;
    }

    QFile f(m_dir.filePath(QStringLiteral("preview.slang")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(QStringLiteral("Could not write the preview shader: ") + f.errorString());
        return;
    }
    f.write(m_pending.toUtf8());
    f.close();

    m_running = 2;
    launch("vsMain", "vertex", "preview.vert");
    launch("psMain", "fragment", "preview.frag");
}

void PreviewCompiler::launch(const char* entry, const char* stage, const char* output)
{
    auto* p = new QProcess(this);
    p->setWorkingDirectory(m_dir.path());
    p->setProcessChannelMode(QProcess::MergedChannels);

    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus status) {
        const QString text = QString::fromUtf8(p->readAll());
        if (code != 0 || status != QProcess::NormalExit)
            m_log += text.isEmpty() ? QStringLiteral("slangc failed.\n") : text;
        p->deleteLater();
        stageDone();
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_log += QStringLiteral("Could not start %1\n").arg(m_slangc);
        p->deleteLater();
        stageDone();
    });

    p->start(m_slangc, {QStringLiteral("preview.slang"),
                        QStringLiteral("-entry"), QString::fromLatin1(entry),
                        QStringLiteral("-stage"), QString::fromLatin1(stage),
                        QStringLiteral("-target"), QStringLiteral("glsl"),
                        QStringLiteral("-profile"), QStringLiteral("glsl_450"),
                        QStringLiteral("-matrix-layout-row-major"),
                        QStringLiteral("-I"), m_dir.path(),
                        QStringLiteral("-o"), QString::fromLatin1(output)});
}

void PreviewCompiler::stageDone()
{
    if (--m_running > 0) return;

    if (m_hasPending) {
        start();
        return;
    }
    if (!m_log.isEmpty()) {
        emit failed(m_log);
        return;
    }
    emit compiled(readText(m_dir.filePath(QStringLiteral("preview.vert"))),
                  readText(m_dir.filePath(QStringLiteral("preview.frag"))));
}

} // namespace sne

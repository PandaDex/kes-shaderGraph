#pragma once

#include <QObject>
#include <QString>
#include <QTemporaryDir>

namespace sne {

// Runs slangc on the exported shader against the preview's kes_common.slang,
// producing GLSL. One compile runs at a time; only the newest pending source is kept.
class PreviewCompiler : public QObject {
    Q_OBJECT
public:
    explicit PreviewCompiler(QObject* parent = nullptr);

    void setSlangc(const QString& path) { m_slangc = path; }
    QString slangc() const { return m_slangc; }

    void compile(const QString& source);

signals:
    void compiled(const QString& vertexGlsl, const QString& fragmentGlsl);
    void failed(const QString& log);

private:
    void start();
    void launch(const char* entry, const char* stage, const char* output);
    void stageDone();

    QString m_slangc;
    QTemporaryDir m_dir;
    QString m_pending;
    bool m_hasPending = false;
    int m_running = 0;
    QString m_log;
};

} // namespace sne

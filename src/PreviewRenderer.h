#pragma once

#include "MaterialSettings.h"

#include <QImage>
#include <QMatrix4x4>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QSize>
#include <array>
#include <memory>

class QOpenGLFramebufferObject;

namespace sne {

enum class PreviewShape { Sphere, Cube, Plane, Torus };

// Draws a mesh with the compiled material into an HDR target, then tonemaps it
// the way the engine does. All calls need the owner's GL context to be current.
class PreviewRenderer : protected QOpenGLExtraFunctions {
public:
    PreviewRenderer();
    ~PreviewRenderer();

    bool initialize(QString* error);
    void shutdown();

    bool setProgram(const QString& vertexGlsl, const QString& fragmentGlsl, QString* log);
    void setShape(PreviewShape shape);
    void setMaterial(const MaterialSettings& material) { m_material = material; }
    void setTexture(int slot, const QImage& image);

    void render(GLuint target, const QSize& size, const QVector3D& eye, float time);

private:
    void ensureTargets(const QSize& size);
    void bindMaterial(const QMatrix4x4& viewProj, const QVector3D& eye, float time);

    std::unique_ptr<QOpenGLShaderProgram> m_materialProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_sky;
    std::unique_ptr<QOpenGLShaderProgram> m_tonemap;
    std::unique_ptr<QOpenGLFramebufferObject> m_hdr;
    std::unique_ptr<QOpenGLFramebufferObject> m_resolved;
    std::array<std::unique_ptr<QOpenGLTexture>, MaterialSlotCount> m_textures;
    std::unique_ptr<QOpenGLTexture> m_white;
    QOpenGLVertexArrayObject m_meshVao;
    QOpenGLVertexArrayObject m_emptyVao;
    GLuint m_vertexBuffer = 0;
    GLuint m_indexBuffer = 0;
    GLuint m_uniformBuffers[3] = {};
    int m_indexCount = 0;
    QSize m_size;
    MaterialSettings m_material;
};

} // namespace sne

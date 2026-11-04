#include "PreviewRenderer.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QVector3D>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>
#include <vector>

#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_UNIFORM_BUFFER
#define GL_UNIFORM_BUFFER 0x8A11
#endif

namespace sne {

namespace {

constexpr float kPi = 3.14159265f;
constexpr int kFirstTextureUnit = 3;   // bindings in resource/preview/kes_common.slang

struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];
    float color[4];
};

struct SceneUniforms {
    float viewProj[16];
    float cameraPosition[4];
    float environmentParams[4];
};

struct ModelUniforms {
    float worldViewProj[16];
    float world[16];
};

struct MaterialUniforms {
    float baseColor[4];
    float emissive[4];
    float pbr[4];
    uint32_t flags[4];
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    using Surface = std::function<void(float u, float v, QVector3D& position, QVector3D& normal)>;

    void addGrid(int columns, int rows, const Surface& surface)
    {
        const uint32_t base = uint32_t(vertices.size());
        for (int y = 0; y <= rows; ++y) {
            for (int x = 0; x <= columns; ++x) {
                const float u = float(x) / columns, v = float(y) / rows;
                QVector3D p, n;
                surface(u, v, p, n);
                vertices.push_back({{p.x(), p.y(), p.z()}, {n.x(), n.y(), n.z()}, {u, v}, {1, 1, 1, 1}});
            }
        }
        const uint32_t stride = uint32_t(columns + 1);
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < columns; ++x) {
                const uint32_t i = base + uint32_t(y) * stride + uint32_t(x);
                addTriangle(i, i + stride, i + 1);
                addTriangle(i + 1, i + stride, i + stride + 1);
            }
        }
    }

    // Winds every triangle to face along its normals, so surfaces don't have to care.
    void addTriangle(uint32_t a, uint32_t b, uint32_t c)
    {
        auto pos = [&](uint32_t i) { return QVector3D(vertices[i].position[0], vertices[i].position[1], vertices[i].position[2]); };
        const QVector3D face = QVector3D::crossProduct(pos(b) - pos(a), pos(c) - pos(a));
        if (face.lengthSquared() < 1e-12f) return;
        const Vertex& va = vertices[a];
        const QVector3D n(va.normal[0], va.normal[1], va.normal[2]);
        if (QVector3D::dotProduct(face, n) < 0.0f) std::swap(b, c);
        indices.insert(indices.end(), {a, b, c});
    }
};

Mesh buildMesh(PreviewShape shape)
{
    Mesh mesh;
    switch (shape) {
    case PreviewShape::Sphere:
        mesh.addGrid(96, 64, [](float u, float v, QVector3D& p, QVector3D& n) {
            const float theta = u * 2.0f * kPi, phi = v * kPi;
            n = QVector3D(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
            p = n;
        });
        break;
    case PreviewShape::Cube: {
        const QVector3D faces[6][3] = {
            {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},   {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}, {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},
            {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},  {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},  {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
        };
        for (const auto& f : faces) {
            mesh.addGrid(1, 1, [&f](float u, float v, QVector3D& p, QVector3D& n) {
                n = f[0];
                p = (f[0] + f[1] * (u * 2.0f - 1.0f) + f[2] * (1.0f - v * 2.0f)) * 0.75f;
            });
        }
        break;
    }
    case PreviewShape::Plane:
        mesh.addGrid(1, 1, [](float u, float v, QVector3D& p, QVector3D& n) {
            n = QVector3D(0, 0, 1);
            p = QVector3D(u * 2.4f - 1.2f, 1.2f - v * 2.4f, 0);
        });
        break;
    case PreviewShape::Torus:
        mesh.addGrid(128, 48, [](float u, float v, QVector3D& p, QVector3D& n) {
            const float a = u * 2.0f * kPi, b = v * 2.0f * kPi;
            const QVector3D ring(std::cos(a), 0, std::sin(a));
            n = ring * std::cos(b) + QVector3D(0, std::sin(b), 0);
            p = ring * 0.85f + n * 0.38f;
        });
        break;
    }
    return mesh;
}

const char* const kFullscreenVertex = R"(#version 450 core
out vec2 vNdc;
void main()
{
    vNdc = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    gl_Position = vec4(vNdc, 0.0, 1.0);
}
)";

// Same gradient as KesPreviewSky in resource/preview/kes_common.slang.
const char* const kSkyFragment = R"(#version 450 core
in vec2 vNdc;
out vec4 fragColor;
uniform mat4 uInverseViewProj;
uniform vec3 uEye;
void main()
{
    vec4 far = uInverseViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 d = normalize(far.xyz / far.w - uEye);
    vec3 sky = mix(vec3(0.75, 0.80, 0.85), vec3(0.25, 0.42, 0.75), clamp(d.y, 0.0, 1.0));
    fragColor = vec4(mix(vec3(0.18, 0.16, 0.14), sky, smoothstep(-0.02, 0.02, d.y)), 1.0);
}
)";

// engine/shaders/blit.slang without the dither.
const char* const kTonemapFragment = R"(#version 450 core
in vec2 vNdc;
out vec4 fragColor;
uniform sampler2D uScene;

// Column by column: the rows of the matrices in blit.slang are these columns.
const mat3 kAcesInput = mat3(0.59719, 0.07600, 0.02840,
                             0.35458, 0.90834, 0.13383,
                             0.04823, 0.01566, 0.83777);
const mat3 kAcesOutput = mat3(1.60475, -0.10208, -0.00327,
                              -0.53108, 1.10813, -0.07276,
                              -0.07367, -0.00605, 1.07602);

vec3 acesFitted(vec3 color)
{
    color = kAcesInput * color;
    vec3 a = color * (color + 0.0245786) - 0.000090537;
    vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
    return clamp(kAcesOutput * (a / b), 0.0, 1.0);
}

vec3 linearToSrgb(vec3 c)
{
    return mix(1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, c * 12.92, lessThanEqual(c, vec3(0.0031308)));
}

void main()
{
    vec3 hdr = max(texture(uScene, vNdc * 0.5 + 0.5).rgb, 0.0);
    fragColor = vec4(linearToSrgb(acesFitted(hdr)), 1.0);
}
)";

std::unique_ptr<QOpenGLShaderProgram> buildProgram(const QString& vertex, const QString& fragment, QString* log)
{
    auto program = std::make_unique<QOpenGLShaderProgram>();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex) ||
        !program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment) || !program->link()) {
        if (log) *log = program->log();
        return nullptr;
    }
    return program;
}

std::unique_ptr<QOpenGLTexture> uploadTexture(const QImage& image, bool srgb)
{
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    auto texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    texture->setFormat(srgb ? QOpenGLTexture::SRGB8_Alpha8 : QOpenGLTexture::RGBA8_UNorm);
    texture->setSize(rgba.width(), rgba.height());
    texture->setMipLevels(texture->maximumMipLevels());
    texture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    texture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, rgba.constBits());
    texture->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
    texture->setWrapMode(QOpenGLTexture::Repeat);
    return texture;
}

} // namespace

PreviewRenderer::PreviewRenderer() = default;
PreviewRenderer::~PreviewRenderer() = default;

bool PreviewRenderer::initialize(QString* error)
{
    initializeOpenGLFunctions();

    m_sky = buildProgram(kFullscreenVertex, kSkyFragment, error);
    m_tonemap = buildProgram(kFullscreenVertex, kTonemapFragment, error);
    if (!m_sky || !m_tonemap) return false;

    m_meshVao.create();
    m_emptyVao.create();
    glGenBuffers(1, &m_vertexBuffer);
    glGenBuffers(1, &m_indexBuffer);
    glGenBuffers(3, m_uniformBuffers);

    QImage white(1, 1, QImage::Format_RGBA8888);
    white.fill(Qt::white);
    m_white = uploadTexture(white, false);

    setShape(PreviewShape::Sphere);
    return true;
}

void PreviewRenderer::shutdown()
{
    m_materialProgram.reset();
    m_sky.reset();
    m_tonemap.reset();
    m_hdr.reset();
    m_resolved.reset();
    m_white.reset();
    for (auto& t : m_textures) t.reset();
    m_size = {};
    m_meshVao.destroy();
    m_emptyVao.destroy();
    if (m_vertexBuffer) {
        glDeleteBuffers(1, &m_vertexBuffer);
        glDeleteBuffers(1, &m_indexBuffer);
        glDeleteBuffers(3, m_uniformBuffers);
        m_vertexBuffer = m_indexBuffer = 0;
    }
}

bool PreviewRenderer::setProgram(const QString& vertexGlsl, const QString& fragmentGlsl, QString* log)
{
    auto program = buildProgram(vertexGlsl, fragmentGlsl, log);
    if (!program) return false;
    m_materialProgram = std::move(program);
    return true;
}

void PreviewRenderer::setShape(PreviewShape shape)
{
    const Mesh mesh = buildMesh(shape);
    m_indexCount = int(mesh.indices.size());

    m_meshVao.bind();
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(mesh.vertices.size() * sizeof(Vertex)), mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(mesh.indices.size() * sizeof(uint32_t)), mesh.indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, color)));
    m_meshVao.release();
}

void PreviewRenderer::setTexture(int slot, const QImage& image)
{
    m_textures[slot].reset();
    if (!image.isNull()) m_textures[slot] = uploadTexture(image, materialSlotInfo(slot).srgb);
}

void PreviewRenderer::ensureTargets(const QSize& size)
{
    if (m_hdr && m_size == size) return;
    m_size = size;

    QOpenGLFramebufferObjectFormat format;
    format.setInternalTextureFormat(GL_RGBA16F);
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    format.setSamples(4);
    m_hdr = std::make_unique<QOpenGLFramebufferObject>(size, format);

    format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    format.setSamples(0);
    m_resolved = std::make_unique<QOpenGLFramebufferObject>(size, format);
}

void PreviewRenderer::bindMaterial(const QMatrix4x4& viewProj, const QVector3D& eye, float time)
{
    const MaterialSettings& m = m_material;

    // QMatrix4x4 is column-major, which is how Slang's GLSL reads these blocks.
    SceneUniforms scene = {};
    std::memcpy(scene.viewProj, viewProj.constData(), sizeof(scene.viewProj));
    scene.cameraPosition[0] = eye.x();
    scene.cameraPosition[1] = eye.y();
    scene.cameraPosition[2] = eye.z();
    scene.environmentParams[1] = 1.0f;
    scene.environmentParams[2] = time;

    ModelUniforms model = {};
    const QMatrix4x4 world;
    std::memcpy(model.worldViewProj, viewProj.constData(), sizeof(model.worldViewProj));
    std::memcpy(model.world, world.constData(), sizeof(model.world));

    MaterialUniforms material = {};
    for (int i = 0; i < 4; ++i) material.baseColor[i] = float(m.baseColor[i]);
    for (int i = 0; i < 3; ++i) material.emissive[i] = float(m.emissive[i] * m.emissiveIntensity);
    material.emissive[3] = float(m.alphaCutoff);
    material.pbr[0] = float(m.metallic);
    material.pbr[1] = float(m.roughness);
    material.pbr[2] = float(m.normalScale);
    material.pbr[3] = float(m.occlusionStrength);
    material.flags[0] = (m.lit ? 1u : 0u) | (m.transparent ? 2u : 0u) | (m.doubleSided ? 4u : 0u);

    for (int i = 0; i < MaterialSlotCount; ++i) {
        // MaterialFlag_HasBaseTexture and the following bits (engine/material.h).
        if (m_textures[i]) material.flags[0] |= 8u << i;
        QOpenGLTexture* texture = m_textures[i] ? m_textures[i].get() : m_white.get();
        texture->bind(uint(kFirstTextureUnit + i));
    }

    const std::pair<const void*, size_t> blocks[3] = {
        {&scene, sizeof(scene)}, {&model, sizeof(model)}, {&material, sizeof(material)}};
    for (GLuint i = 0; i < 3; ++i) {
        glBindBuffer(GL_UNIFORM_BUFFER, m_uniformBuffers[i]);
        glBufferData(GL_UNIFORM_BUFFER, GLsizeiptr(blocks[i].second), blocks[i].first, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, i, m_uniformBuffers[i]);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void PreviewRenderer::render(GLuint target, const QSize& size, const QVector3D& eye, float time)
{
    if (size.isEmpty()) return;
    ensureTargets(size);

    QMatrix4x4 view;
    view.lookAt(eye, QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    QMatrix4x4 projection;
    projection.perspective(40.0f, float(size.width()) / float(size.height()), 0.05f, 50.0f);
    const QMatrix4x4 viewProj = projection * view;

    m_hdr->bind();
    glViewport(0, 0, size.width(), size.height());
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    m_emptyVao.bind();
    m_sky->bind();
    m_sky->setUniformValue("uInverseViewProj", viewProj.inverted());
    m_sky->setUniformValue("uEye", eye);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    m_sky->release();

    if (m_materialProgram) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        if (!m_material.doubleSided) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
        }
        if (m_material.transparent) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }
        bindMaterial(viewProj, eye, time);
        m_materialProgram->bind();
        m_meshVao.bind();
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
        m_materialProgram->release();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
    }

    QOpenGLFramebufferObject::blitFramebuffer(m_resolved.get(), m_hdr.get(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glViewport(0, 0, size.width(), size.height());
    m_emptyVao.bind();
    m_tonemap->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_resolved->texture());
    m_tonemap->setUniformValue("uScene", 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    m_tonemap->release();
    m_emptyVao.release();
}

} // namespace sne

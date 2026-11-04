#include "PreviewWidget.h"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QVector3D>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace sne {

PreviewWidget::PreviewWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    setMinimumSize(160, 160);
    m_frameTimer.setInterval(33);
    connect(&m_frameTimer, &QTimer::timeout, this, [this] { update(); });
    m_clock.start();
}

PreviewWidget::~PreviewWidget()
{
    makeCurrent();
    releaseGL();
    doneCurrent();
}

void PreviewWidget::releaseGL()
{
    if (m_ready) m_renderer.shutdown();
    m_ready = false;
}

void PreviewWidget::setProgram(const QString& vertexGlsl, const QString& fragmentGlsl)
{
    m_vertex = vertexGlsl;
    m_fragment = fragmentGlsl;
    m_programDirty = true;
    update();
}

void PreviewWidget::setShape(PreviewShape shape)
{
    m_shape = shape;
    m_shapeDirty = true;
    update();
}

void PreviewWidget::setMaterial(const MaterialSettings& material)
{
    m_renderer.setMaterial(material);
    update();
}

void PreviewWidget::setTexture(int slot, const QImage& image)
{
    m_images[slot] = image;
    m_imageDirty[slot] = true;
    update();
}

void PreviewWidget::setAnimated(bool animated)
{
    if (animated) m_frameTimer.start();
    else m_frameTimer.stop();
}

void PreviewWidget::initializeGL()
{
    // Reparenting (floating the dock) can replace the context.
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] {
        makeCurrent();
        releaseGL();
        doneCurrent();
    });

    QString error;
    m_ready = m_renderer.initialize(&error);
    if (!m_ready) {
        emit rendererFailed(error);
        return;
    }
    m_programDirty = !m_vertex.isEmpty();
    m_shapeDirty = true;
    m_imageDirty.fill(true);
}

void PreviewWidget::paintGL()
{
    if (!m_ready) return;

    if (m_shapeDirty) {
        m_renderer.setShape(m_shape);
        m_shapeDirty = false;
    }
    for (int i = 0; i < MaterialSlotCount; ++i) {
        if (!m_imageDirty[i]) continue;
        m_renderer.setTexture(i, m_images[i]);
        m_imageDirty[i] = false;
    }
    if (m_programDirty) {
        m_programDirty = false;
        QString log;
        if (!m_renderer.setProgram(m_vertex, m_fragment, &log)) emit shaderRejected(log);
    }

    const qreal dpr = devicePixelRatioF();
    const QSize size(qRound(width() * dpr), qRound(height() * dpr));
    m_renderer.render(defaultFramebufferObject(), size, eye(), float(m_clock.elapsed()) / 1000.0f);
}

QVector3D PreviewWidget::eye() const
{
    return QVector3D(std::sin(m_yaw) * std::cos(m_pitch), std::sin(m_pitch), std::cos(m_yaw) * std::cos(m_pitch)) * m_distance;
}

void PreviewWidget::mousePressEvent(QMouseEvent* e)
{
    m_lastMouse = e->position().toPoint();
}

void PreviewWidget::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint pos = e->position().toPoint();
    const QPoint delta = pos - m_lastMouse;
    m_lastMouse = pos;
    m_yaw -= float(delta.x()) * 0.01f;
    m_pitch = std::clamp(m_pitch + float(delta.y()) * 0.01f, -1.5f, 1.5f);
    update();
}

void PreviewWidget::wheelEvent(QWheelEvent* e)
{
    m_distance = std::clamp(m_distance * std::pow(0.999f, float(e->angleDelta().y())), 1.6f, 12.0f);
    update();
}

} // namespace sne

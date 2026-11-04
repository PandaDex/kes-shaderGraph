#pragma once

#include "PreviewRenderer.h"

#include <QElapsedTimer>
#include <QImage>
#include <QOpenGLWidget>
#include <QPoint>
#include <QTimer>
#include <array>

namespace sne {

class PreviewWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget* parent = nullptr);
    ~PreviewWidget() override;

    void setProgram(const QString& vertexGlsl, const QString& fragmentGlsl);
    void setShape(PreviewShape shape);
    void setMaterial(const MaterialSettings& material);
    void setTexture(int slot, const QImage& image);
    void setAnimated(bool animated);

signals:
    void shaderRejected(const QString& log);
    void rendererFailed(const QString& log);

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    void releaseGL();
    QVector3D eye() const;

    PreviewRenderer m_renderer;
    bool m_ready = false;

    // Kept so everything can be re-applied if the context is recreated, and
    // so calls before the first paint or without a current context can wait.
    QString m_vertex, m_fragment;
    PreviewShape m_shape = PreviewShape::Sphere;
    std::array<QImage, MaterialSlotCount> m_images;
    bool m_programDirty = false;
    bool m_shapeDirty = false;
    std::array<bool, MaterialSlotCount> m_imageDirty{};

    float m_yaw = 0.6f;
    float m_pitch = 0.3f;
    float m_distance = 3.6f;
    QPoint m_lastMouse;
    QTimer m_frameTimer;
    QElapsedTimer m_clock;
};

} // namespace sne

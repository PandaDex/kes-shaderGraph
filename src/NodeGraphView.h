#pragma once

#include <QGraphicsView>

namespace sne {

class NodeGraphScene;

class NodeGraphView : public QGraphicsView {
    Q_OBJECT
public:
    static constexpr const char* kNodeMime = "application/x-sne-node";

    explicit NodeGraphView(NodeGraphScene* scene, QWidget* parent = nullptr);

    void frameAll();
    void frameSelection();
    void resetZoom();
    void zoomBy(double factor);
    double zoomLevel() const { return transform().m11(); }
    QPointF viewCenterScene() const;
    QPointF cursorScenePos() const;

signals:
    void contextMenuRequestedAt(const QPointF& scenePos, const QPoint& globalPos);
    void nodeDropped(const QString& type, const QPointF& scenePos);
    void zoomChanged(double zoom);

protected:
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void drawBackground(QPainter*, const QRectF&) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragMoveEvent(QDragMoveEvent*) override;
    void dropEvent(QDropEvent*) override;

private:
    void fitRect(const QRectF& r);

    NodeGraphScene* m_scene;
    bool m_panning = false;
    bool m_spaceDown = false;
    QPoint m_lastPan;
    QPoint m_pressPos;
    bool m_rightMoved = false;
};

} // namespace sne

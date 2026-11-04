#include "NodeGraphView.h"

#include "NodeGraphScene.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QPainter>
#include <QScrollBar>
#include <QSurfaceFormat>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace sne {

namespace {
constexpr double kMinZoom = 0.15;
constexpr double kMaxZoom = 3.0;
}

NodeGraphView::NodeGraphView(NodeGraphScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent), m_scene(scene)
{
    if (!qEnvironmentVariableIsSet("SNE_SOFTWARE_RENDER")) {
        auto* gl = new QOpenGLWidget;
        QSurfaceFormat fmt = gl->format();
        fmt.setSamples(4);
        gl->setFormat(fmt);
        setViewport(gl);
    }
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);
    setCacheMode(QGraphicsView::CacheNone);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::RubberBandDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    centerOn(0, 0);
}

QPointF NodeGraphView::viewCenterScene() const { return mapToScene(viewport()->rect().center()); }

QPointF NodeGraphView::cursorScenePos() const
{
    const QPoint p = viewport()->mapFromGlobal(QCursor::pos());
    return viewport()->rect().contains(p) ? mapToScene(p) : viewCenterScene();
}

void NodeGraphView::zoomBy(double factor)
{
    const double target = std::clamp(zoomLevel() * factor, kMinZoom, kMaxZoom);
    factor = target / zoomLevel();
    scale(factor, factor);
    emit zoomChanged(zoomLevel());
    viewport()->update();
}

void NodeGraphView::resetZoom()
{
    const QPointF c = viewCenterScene();
    resetTransform();
    centerOn(c);
    emit zoomChanged(1.0);
}

void NodeGraphView::fitRect(const QRectF& r)
{
    if (r.isNull()) { resetTransform(); centerOn(0, 0); emit zoomChanged(1.0); return; }
    fitInView(r.adjusted(-80, -80, 80, 80), Qt::KeepAspectRatio);
    const double z = zoomLevel();
    if (z > 1.5 || z < kMinZoom) {
        const double clamped = std::clamp(z, kMinZoom, 1.5);
        scale(clamped / z, clamped / z);
        centerOn(r.center());
    }
    emit zoomChanged(zoomLevel());
}

void NodeGraphView::frameAll() { fitRect(m_scene->nodesBoundingRect(false)); }
void NodeGraphView::frameSelection()
{
    const QRectF r = m_scene->nodesBoundingRect(true);
    if (r.isNull()) frameAll(); else fitRect(r);
}

void NodeGraphView::wheelEvent(QWheelEvent* e)
{
    const double delta = e->angleDelta().y();
    if (delta == 0) { e->ignore(); return; }
    zoomBy(std::pow(1.0015, delta));
    e->accept();
}

void NodeGraphView::mousePressEvent(QMouseEvent* e)
{
    const bool panGesture = e->button() == Qt::MiddleButton || (m_spaceDown && e->button() == Qt::LeftButton);
    if (panGesture) {
        m_panning = true;
        m_lastPan = e->pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    if (e->button() == Qt::RightButton) {
        m_pressPos = e->pos();
        m_rightMoved = false;
    }
    QGraphicsView::mousePressEvent(e);
}

void NodeGraphView::mouseMoveEvent(QMouseEvent* e)
{
    if (m_panning) {
        const QPoint d = e->pos() - m_lastPan;
        m_lastPan = e->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        e->accept();
        return;
    }
    if ((e->buttons() & Qt::RightButton) && (e->pos() - m_pressPos).manhattanLength() > 4) m_rightMoved = true;
    QGraphicsView::mouseMoveEvent(e);
}

void NodeGraphView::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_panning && (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton)) {
        m_panning = false;
        setCursor(m_spaceDown ? Qt::OpenHandCursor : Qt::ArrowCursor);
        e->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
}

void NodeGraphView::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        m_spaceDown = true;
        setDragMode(QGraphicsView::NoDrag);
        setCursor(Qt::OpenHandCursor);
        e->accept();
        return;
    }
    QGraphicsView::keyPressEvent(e);
}

void NodeGraphView::keyReleaseEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        m_spaceDown = false;
        setDragMode(QGraphicsView::RubberBandDrag);
        if (!m_panning) setCursor(Qt::ArrowCursor);
        e->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(e);
}

void NodeGraphView::contextMenuEvent(QContextMenuEvent* e)
{
    if (m_rightMoved) { e->accept(); return; }
    emit contextMenuRequestedAt(mapToScene(e->pos()), e->globalPos());
    e->accept();
}

void NodeGraphView::drawBackground(QPainter* p, const QRectF& rect)
{
    const QTransform t = p->worldTransform();
    const double z = t.m11();
    const QRectF dev = t.mapRect(rect);

    p->save();
    p->resetTransform();
    p->setRenderHint(QPainter::Antialiasing, false);
    p->fillRect(dev, QColor(0x22, 0x22, 0x26));

    constexpr int minor = 20, major = 100;
    auto drawGrid = [&](int step, const QColor& c, bool skipMajor) {
        const int x0 = int(std::floor(rect.left() / step)) * step;
        const int y0 = int(std::floor(rect.top() / step)) * step;
        for (int x = x0; x < rect.right(); x += step) {
            if (skipMajor && x % major == 0) continue;
            p->fillRect(QRectF(std::floor(x * z + t.dx()), dev.top(), 1, dev.height()), c);
        }
        for (int y = y0; y < rect.bottom(); y += step) {
            if (skipMajor && y % major == 0) continue;
            p->fillRect(QRectF(dev.left(), std::floor(y * z + t.dy()), dev.width(), 1), c);
        }
    };
    if (z > 0.45) drawGrid(minor, QColor(255, 255, 255, 10), true);
    drawGrid(major, QColor(255, 255, 255, z > 0.3 ? 22 : 12), false);

    const QColor axis(255, 255, 255, 40);
    if (rect.left() < 0 && rect.right() > 0) p->fillRect(QRectF(std::floor(t.dx()) - 1, dev.top(), 2, dev.height()), axis);
    if (rect.top() < 0 && rect.bottom() > 0) p->fillRect(QRectF(dev.left(), std::floor(t.dy()) - 1, dev.width(), 2), axis);
    p->restore();
}

void NodeGraphView::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasFormat(kNodeMime)) e->acceptProposedAction();
    else QGraphicsView::dragEnterEvent(e);
}

void NodeGraphView::dragMoveEvent(QDragMoveEvent* e)
{
    if (e->mimeData()->hasFormat(kNodeMime)) e->acceptProposedAction();
    else QGraphicsView::dragMoveEvent(e);
}

void NodeGraphView::dropEvent(QDropEvent* e)
{
    if (!e->mimeData()->hasFormat(kNodeMime)) { QGraphicsView::dropEvent(e); return; }
    const QString type = QString::fromUtf8(e->mimeData()->data(kNodeMime));
    emit nodeDropped(type, mapToScene(e->position().toPoint()));
    e->acceptProposedAction();
}

} // namespace sne

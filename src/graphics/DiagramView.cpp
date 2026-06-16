#include "DiagramView.h"
#include <QWheelEvent>
#include <QPainter>
#include <qmath.h>

DiagramView::DiagramView(QWidget *parent)
    : QGraphicsView(parent)
{
    // 鼠标抓手拖动平移支持
    setDragMode(QGraphicsView::ScrollHandDrag);
    
    // 渲染提示：抗锯齿、图像平滑缩放
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // 隐藏默认滚动条
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 定焦在鼠标指针下方缩放
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void DiagramView::resetZoom()
{
    resetTransform();
    if (scene()) {
        centerOn(scene()->sceneRect().center());
    }
}

void DiagramView::fitToContent()
{
    if (!scene()) return;
    
    QRectF sceneRect = scene()->sceneRect();
    if (sceneRect.isEmpty()) return;
    
    // 调用视图内置函数适应矩形，周围预留 40 像素的内边距
    fitInView(sceneRect, Qt::KeepAspectRatio);
    
    // 可选：限制缩放比例最大不要超过 1.2 倍，防止极小的图被拉得非常巨大
    double currentScale = transform().m11();
    if (currentScale > 1.2) {
        resetTransform();
        scale(1.2, 1.2);
        centerOn(sceneRect.center());
    }
}

void DiagramView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        double angle = event->angleDelta().y();
        double factor = 1.15;
        if (angle < 0) {
            factor = 1.0 / factor;
        }
        
        double currentScale = transform().m11();
        // 限制缩放区间为 15% 到 800%
        if ((factor > 1.0 && currentScale < 8.0) || (factor < 1.0 && currentScale > 0.15)) {
            scale(factor, factor);
        }
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DiagramView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->save();
    
    // 关闭抗锯齿，极大提升画直线网格时的 CPU/GPU 性能
    painter->setRenderHint(QPainter::Antialiasing, false);
    
    // 1. 填充极清亮的浅灰白底色
    painter->setBrush(QColor("#f9fafb"));
    painter->setPen(Qt::NoPen);
    painter->drawRect(rect);
    
    // 2. 绘制微弱的点状工业网格
    QPen pen(QColor("#e5e7eb"), 1.0, Qt::DotLine);
    painter->setPen(pen);
    
    const int gridStep = 40; // 40 像素一个网格
    
    // 计算对齐当前视口裁剪区的网格线起点
    qreal left = qFloor(rect.left() / gridStep) * gridStep;
    qreal top = qFloor(rect.top() / gridStep) * gridStep;
    
    for (qreal x = left; x < rect.right(); x += gridStep) {
        painter->drawLine(x, rect.top(), x, rect.bottom());
    }
    for (qreal y = top; y < rect.bottom(); y += gridStep) {
        painter->drawLine(rect.left(), y, rect.right(), y);
    }
    
    painter->restore();
}

#include "plantuml_view.h"
#include <QWheelEvent>

PlantUmlView::PlantUmlView(QWidget *parent)
    : QGraphicsView(parent)
{
    // 开启鼠标左键直接拖拽平移模式
    setDragMode(QGraphicsView::ScrollHandDrag);

    // 开启高品质矢量渲染抗锯齿
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);

    // 隐藏默认滚动条，使界面更加一体化、清爽
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 设定缩放锚点为鼠标所在位置 (最关键的以鼠标为中心缩放)
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    
    // 设定视口更新模式为完整更新，防止快速拖动时出现图像残影
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void PlantUmlView::wheelEvent(QWheelEvent *event)
{
    // Ctrl + 滚轮触发矢量无极缩放
    if (event->modifiers() & Qt::ControlModifier) {
        double angle = event->angleDelta().y();
        double factor = 1.15;
        if (angle < 0) {
            factor = 1.0 / factor;
        }
        
        // 限制不要缩得太小或太大 (根据当前变换矩阵的 m11 元素)
        double currentScale = transform().m11();
        if ((factor > 1.0 && currentScale < 10.0) || (factor < 1.0 && currentScale > 0.1)) {
            scale(factor, factor);
        }
        event->accept();
    } else {
        // 普通滚轮行为（比如上下滚动）
        QGraphicsView::wheelEvent(event);
    }
}

void PlantUmlView::resetZoom()
{
    // 重置变换矩阵为单位矩阵 (100% 缩放)
    resetTransform();
    
    // 如果有场景，将视口居中到场景的中心点
    if (scene()) {
        centerOn(scene()->sceneRect().center());
    }
}

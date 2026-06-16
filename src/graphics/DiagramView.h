#pragma once

#include <QGraphicsView>

class QWheelEvent;
class QPainter;

class DiagramView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit DiagramView(QWidget *parent = nullptr);

    // 重置变换矩阵为 1.0 (原图大小)
    void resetZoom();

    // 缩放视口使其完整容纳场景内容
    void fitToContent();

protected:
    // Ctrl + 滚轮定点缩放
    void wheelEvent(QWheelEvent *event) override;

    // 绘制工业质感点状网格背景
    void drawBackground(QPainter *painter, const QRectF &rect) override;
};

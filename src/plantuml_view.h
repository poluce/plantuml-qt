#pragma once

#include <QGraphicsView>

class QWheelEvent;

class PlantUmlView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit PlantUmlView(QWidget *parent = nullptr);

    // 重置缩放比例并居中显示
    void resetZoom();

protected:
    // 拦截 Ctrl + 滚轮实现以鼠标指针为中心的无缝矢量缩放
    void wheelEvent(QWheelEvent *event) override;
};

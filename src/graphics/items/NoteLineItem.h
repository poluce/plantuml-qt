#pragma once

#include <QGraphicsItem>
#include "../../render_model/RenderDocument.h"
#include "../../render_model/RenderTheme.h"
#include "ClassBoxItem.h"

class NoteLineItem : public QGraphicsItem
{
public:
    NoteLineItem(const RenderEdge &edge, const RenderTheme &theme);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    
    // 核心绘制：只画单纯细虚线，不带任何关系箭头
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setNodes(QGraphicsItem *fromNode, QGraphicsItem *toNode);
    
    // 物理动态追踪，调用 ClassBox 暴露出来的 portPoint
    void trackNodes();

    QString fromNodeId() const { return m_edge.fromNodeId; }
    QString toNodeId() const { return m_edge.toNodeId; }

private:
    RenderEdge m_edge;
    RenderTheme m_theme;
    QGraphicsItem *m_fromItem;
    QGraphicsItem *m_toItem;
};

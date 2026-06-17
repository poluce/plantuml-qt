#pragma once

#include <QGraphicsItem>
#include <QPainterPath>
#include "../../render_model/RenderDocument.h"
#include "../../render_model/RenderTheme.h"

class MessageArrowItem : public QGraphicsItem
{
public:
    MessageArrowItem(const RenderEdge &edge, const RenderTheme &theme);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;

    // 执行消息线、文字和箭头的绘制
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    SourceLocation location() const { return m_edge.location; }

private:
    // 绘制箭头辅助函数 (支持开口或实心)
    void drawArrowHead(QPainter *painter, const QPointF &tip, bool pointsRight, bool isSolid);

    RenderEdge m_edge;
    RenderTheme m_theme;
};

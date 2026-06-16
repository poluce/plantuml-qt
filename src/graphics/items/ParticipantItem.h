#pragma once

#include <QGraphicsItem>
#include "../../render_model/RenderDocument.h"
#include "../../render_model/RenderTheme.h"

class ParticipantItem : public QGraphicsItem
{
public:
    ParticipantItem(const RenderNode &node, const RenderTheme &theme);

    // 计算图元所占的几何总包围盒
    QRectF boundingRect() const override;

    // 执行实际绘制 (包括顶底双框、文字和生命线)
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QString id() const { return m_node.id; }
    SourceLocation location() const { return m_node.location; }

private:
    RenderNode m_node;
    RenderTheme m_theme;
};

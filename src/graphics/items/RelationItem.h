#pragma once

#include <QGraphicsItem>
#include <QPainterPath>
#include "../../render_model/RenderDocument.h"
#include "../../render_model/RenderTheme.h"

class RelationItem : public QGraphicsItem
{
public:
    RelationItem(const RenderEdge &edge, const RenderTheme &theme);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;

    // 执行类图继承线、关联线及相应 UML 箭头的绘制
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    SourceLocation location() const { return m_edge.location; }

    // 新增：追踪与绑定起止节点的接口
    void setNodes(QGraphicsItem *fromNode, QGraphicsItem *toNode);
    void trackNodes();

    // 新增：提取双态布局及端点信息接口
    QString fromNodeId() const { return m_edge.fromNodeId; }
    QString toNodeId() const { return m_edge.toNodeId; }
    QString label() const { return m_edge.label; }
    QPointF initialStart() const { return m_initialStart; }
    QPointF initialEnd() const { return m_initialEnd; }
    QPointF currentStart() const { return m_edge.startPoint; }
    QPointF currentEnd() const { return m_edge.endPoint; }
    QRectF sceneRect() const { return sceneBoundingRect(); }

private:
    // 绘制旋转自适应的各类 UML 箭头和几何符号
    void drawRotatedArrow(QPainter *painter, const QPointF &tip, double rad, RenderEdgeKind kind);

    RenderEdge m_edge;
    RenderTheme m_theme;
    QGraphicsItem *m_fromItem;
    QGraphicsItem *m_toItem;

    QPointF m_initialStart;
    QPointF m_initialEnd;
};

#pragma once

#include <QGraphicsItem>
#include "../../render_model/RenderDocument.h"
#include "../../render_model/RenderTheme.h"

#include <QVector>

enum class BoxSide {
    Top,
    Bottom,
    Left,
    Right
};

class ClassBoxItem : public QGraphicsItem
{
public:
    ClassBoxItem(const RenderNode &node, const RenderTheme &theme);

    QRectF boundingRect() const override;
    
    // 执行类名卡片背景、分割线以及成员文本列表的绘制
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QString id() const { return m_node.id; }
    SourceLocation location() const { return m_node.location; }
    QRectF sceneRect() const { return sceneBoundingRect(); }

    // 新增：布局反馈状态提取接口
    QPointF initialPos() const { return m_initialPos; }
    QSizeF size() const { return QSizeF(m_node.rect.width(), m_node.rect.height()); }
    QRectF currentRect() const { return QRectF(scenePos(), size()); }
    const RenderNode &node() const { return m_node; }

    // 新增：连线定位接口，提供物理对齐计算
    QPointF portPoint(const QString &portId, const QPointF &fallback, BoxSide side) const;

    // 新增：连线注册与更新接口
    void addEdge(QGraphicsItem *edge);
    void updateEdges(); // 暴露连线更新接口，用于整体移动时被父包图元调用
    const QVector<QGraphicsItem*> &edges() const { return m_edges; }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    RenderNode m_node;
    RenderTheme m_theme;
    QVector<QGraphicsItem*> m_edges; // 新增：保存与当前卡片相连的连线图元指针
    QPointF m_initialPos;            // 保存初始解析后的绝对坐标位置

    QColor m_fillColor;
    QColor m_borderColor;
    double m_penWidth;
    Qt::PenStyle m_penStyle;
};

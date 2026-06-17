#pragma once

#include <QGraphicsItem>
#include "../../render_model/RenderDocument.h"
#include "../../render_model/RenderTheme.h"

#include <QVector>

class ClassBoxItem : public QGraphicsItem
{
public:
    ClassBoxItem(const RenderNode &node, const RenderTheme &theme);

    QRectF boundingRect() const override;
    
    // 执行类名卡片背景、分割线以及成员文本列表的绘制
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QString id() const { return m_node.id; }
    SourceLocation location() const { return m_node.location; }

    // 新增：布局反馈状态提取接口
    QPointF initialPos() const { return m_initialPos; }
    QSizeF size() const { return QSizeF(m_node.rect.width(), m_node.rect.height()); }
    QRectF currentRect() const { return QRectF(scenePos(), size()); }

    // 新增：连线注册与更新接口
    void addEdge(QGraphicsItem *edge);
    void updateEdges(); // 暴露连线更新接口，用于整体移动时被父包图元调用

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    RenderNode m_node;
    RenderTheme m_theme;
    QVector<QGraphicsItem*> m_edges; // 新增：保存与当前卡片相连的连线图元指针
    QPointF m_initialPos;            // 保存初始解析后的绝对坐标位置
};

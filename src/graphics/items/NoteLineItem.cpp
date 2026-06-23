#include "NoteLineItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

NoteLineItem::NoteLineItem(const RenderEdge &edge, const RenderTheme &theme)
    : m_edge(edge)
    , m_theme(theme)
    , m_fromItem(nullptr)
    , m_toItem(nullptr)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QRectF NoteLineItem::boundingRect() const
{
    QRectF rect = QRectF(m_edge.startPoint, m_edge.endPoint).normalized();
    return rect.adjusted(-8, -8, 8, 8);
}

QPainterPath NoteLineItem::shape() const
{
    QPainterPath path;
    path.moveTo(m_edge.startPoint);
    path.lineTo(m_edge.endPoint);
    return path;
}

void NoteLineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // UML 指示线标准：极细的无箭头灰色虚线
    QColor lineColor = m_theme.onSurfaceMutedColor;
    QPen linePen(lineColor, 1.2, Qt::DashLine);
    painter->setPen(linePen);
    painter->drawLine(m_edge.startPoint, m_edge.endPoint);
    
    painter->restore();
}

void NoteLineItem::setNodes(QGraphicsItem *fromNode, QGraphicsItem *toNode)
{
    m_fromItem = fromNode;
    m_toItem = toNode;
}

void NoteLineItem::trackNodes()
{
    if (!m_fromItem || !m_toItem) return;
    prepareGeometryChange();

    QPointF bestFrom = m_fromItem->sceneBoundingRect().center();
    QPointF bestTo = m_toItem->sceneBoundingRect().center();

    // 自动利用 ClassBox 暴露的 portPoint 算法，计算物理场景的完美坐标
    if (const auto *fromBox = dynamic_cast<const ClassBoxItem*>(m_fromItem)) {
        QRectF rFrom = m_fromItem->sceneBoundingRect();
        QRectF rTo = m_toItem->sceneBoundingRect();
        BoxSide fromSide = (rTo.center().x() > rFrom.center().x()) ? BoxSide::Right : BoxSide::Left;
        bestFrom = fromBox->portPoint(m_edge.fromPort, bestFrom, fromSide);
    }
    if (const auto *toBox = dynamic_cast<const ClassBoxItem*>(m_toItem)) {
        QRectF rFrom = m_fromItem->sceneBoundingRect();
        QRectF rTo = m_toItem->sceneBoundingRect();
        BoxSide toSide = (rFrom.center().x() > rTo.center().x()) ? BoxSide::Right : BoxSide::Left;
        bestTo = toBox->portPoint(m_edge.toPort, bestTo, toSide);
    }

    m_edge.startPoint = bestFrom;
    m_edge.endPoint = bestTo;
    update();
}

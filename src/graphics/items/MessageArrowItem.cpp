#include "MessageArrowItem.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <qmath.h>

MessageArrowItem::MessageArrowItem(const RenderEdge &edge, const RenderTheme &theme)
    : m_edge(edge)
    , m_theme(theme)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QRectF MessageArrowItem::boundingRect() const
{
    QRectF rect = m_edge.path.isEmpty()
        ? QRectF(m_edge.startPoint, m_edge.endPoint).normalized()
        : m_edge.path.controlPointRect();
    if (m_edge.hasLabelPosition) {
        rect = rect.united(QRectF(m_edge.labelPosition.x() - 70.0, m_edge.labelPosition.y() - 14.0, 140.0, 24.0));
    }
    return rect.adjusted(-12, -28, 12, 28);
}

void MessageArrowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    
    // 1. 判断交互状态
    bool isHovered = option->state & QStyle::State_MouseOver;
    bool isSelected = option->state & QStyle::State_Selected;
    
    // 如果被 hover 或选中，颜色变为稍亮的靛蓝或强调色，否则使用默认主色
    QColor lineColor = (isHovered || isSelected) ? QColor("#4f46e5") : m_theme.primaryColor;
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    // 2. 绘制水平消息线
    QPen linePen;
    linePen.setColor(lineColor);
    linePen.setWidthF(m_theme.lineWidth);
    if (m_edge.kind == RenderEdgeKind::ReplyCall) {
        linePen.setStyle(Qt::DashLine); // 返回消息用虚线
    } else {
        linePen.setStyle(Qt::SolidLine);
    }
    painter->setPen(linePen);
    
    if (m_edge.path.isEmpty()) {
        painter->drawLine(m_edge.startPoint, m_edge.endPoint);
    } else {
        painter->drawPath(m_edge.path);
    }
    
    // 3. 绘制终点处的箭头
    QPointF arrowFrom = m_edge.startPoint;
    QPointF arrowTo = m_edge.endPoint;
    if (m_edge.points.size() >= 2) {
        arrowFrom = m_edge.points[m_edge.points.size() - 2];
        arrowTo = m_edge.points.last();
    }
    bool pointsRight = (arrowFrom.x() < arrowTo.x());
    bool isSolid = (m_edge.kind == RenderEdgeKind::SyncCall);
    drawArrowHead(painter, arrowTo, pointsRight, isSolid);
    
    // 4. 绘制悬浮在连线正中上方的文字标签
    painter->setPen(lineColor);
    
    QFont font = painter->font();
    font.setPointSize(9);
    font.setBold(isSelected); // 选中时文字加粗
    painter->setFont(font);
    
    QRectF textRect;
    if (m_edge.hasLabelPosition) {
        textRect = QRectF(m_edge.labelPosition.x() - 70.0, m_edge.labelPosition.y() - 12.0, 140.0, 20.0);
    } else {
        double xMin = qMin(m_edge.startPoint.x(), m_edge.endPoint.x());
        double w = qAbs(m_edge.startPoint.x() - m_edge.endPoint.x());
        textRect = QRectF(xMin, m_edge.startPoint.y() - 22, w, 18);
    }
    painter->drawText(textRect, Qt::AlignCenter, m_edge.label);
    
    painter->restore();
}

void MessageArrowItem::drawArrowHead(QPainter *painter, const QPointF &tip, bool pointsRight, bool isSolid)
{
    double size = m_theme.arrowSize;
    double dx = pointsRight ? -size : size;
    
    QPointF p1(tip.x() + dx, tip.y() - size / 2.0);
    QPointF p2(tip.x() + dx, tip.y() + size / 2.0);
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    if (isSolid) {
        // 同步消息：绘制实心闭合三角形
        QPainterPath path;
        path.moveTo(tip);
        path.lineTo(p1);
        path.lineTo(p2);
        path.closeSubpath();
        
        painter->setBrush(QBrush(painter->pen().color()));
        painter->drawPath(path);
    } else {
        // 返回消息：仅绘制开口线段
        painter->drawLine(tip, p1);
        painter->drawLine(tip, p2);
    }
    
    painter->restore();
}

QPainterPath MessageArrowItem::shape() const
{
    QPainterPath path;
    if (m_edge.path.isEmpty()) {
        path.moveTo(m_edge.startPoint);
        path.lineTo(m_edge.endPoint);
    } else {
        path = m_edge.path;
    }
    
    QPainterPathStroker stroker;
    stroker.setWidth(8.0);
    return stroker.createStroke(path);
}

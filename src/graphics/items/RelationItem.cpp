#include "RelationItem.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <qmath.h>

RelationItem::RelationItem(const RenderEdge &edge, const RenderTheme &theme)
    : m_edge(edge)
    , m_theme(theme)
    , m_fromItem(nullptr)
    , m_toItem(nullptr)
    , m_initialStart(edge.startPoint)
    , m_initialEnd(edge.endPoint)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QRectF RelationItem::boundingRect() const
{
    double xMin = qMin(m_edge.startPoint.x(), m_edge.endPoint.x());
    double xMax = qMax(m_edge.startPoint.x(), m_edge.endPoint.x());
    double yMin = qMin(m_edge.startPoint.y(), m_edge.endPoint.y());
    double yMax = qMax(m_edge.startPoint.y(), m_edge.endPoint.y());
    
    double w = xMax - xMin;
    double h = yMax - yMin;
    
    // 留出 15 像素的安全包围盒描边边界
    return QRectF(xMin - 15, yMin - 15, w + 30, h + 30);
}

void RelationItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    
    bool isHovered = option->state & QStyle::State_MouseOver;
    bool isSelected = option->state & QStyle::State_Selected;
    
    QColor lineColor = (isHovered || isSelected) ? QColor("#4f46e5") : m_theme.primaryColor;
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    // 1. 判断并配置实线或虚线画笔样式
    Qt::PenStyle penStyle = Qt::SolidLine;
    if (m_edge.kind == RenderEdgeKind::Realization || m_edge.kind == RenderEdgeKind::Dependency) {
        penStyle = Qt::DashLine;
    }
    
    QPen linePen(lineColor, m_theme.lineWidth, penStyle);
    painter->setPen(linePen);
    
    // 2. 根据起止端点几何夹角，计算自适应线身偏移，防止线段穿入三角/菱形图元内部
    double angle = qAtan2(m_edge.endPoint.y() - m_edge.startPoint.y(), m_edge.endPoint.x() - m_edge.startPoint.x());
    
    QPointF startPt = m_edge.startPoint;
    QPointF endPt = m_edge.endPoint;
    
    if (m_edge.kind == RenderEdgeKind::Inheritance || m_edge.kind == RenderEdgeKind::Realization) {
        double arrowLen = m_theme.arrowSize + 2.0;
        startPt = m_edge.startPoint + QPointF(qCos(angle), qSin(angle)) * arrowLen;
    } else if (m_edge.kind == RenderEdgeKind::Composition || m_edge.kind == RenderEdgeKind::Aggregation) {
        double diamondLen = m_theme.arrowSize * 1.5;
        startPt = m_edge.startPoint + QPointF(qCos(angle), qSin(angle)) * diamondLen;
    }
    
    // 绘制连线线身
    painter->drawLine(startPt, endPt);
    
    // 3. 在对应的端点上绘制矢量旋转对齐的 UML 箭头/符号
    if (m_edge.kind == RenderEdgeKind::Inheritance || 
        m_edge.kind == RenderEdgeKind::Realization || 
        m_edge.kind == RenderEdgeKind::Composition || 
        m_edge.kind == RenderEdgeKind::Aggregation) {
        // 源端（起点）绘制
        drawRotatedArrow(painter, m_edge.startPoint, angle, m_edge.kind);
    } else {
        // 宿端（终点）绘制
        drawRotatedArrow(painter, m_edge.endPoint, angle, m_edge.kind);
    }
    
    // 4. 绘制可能存在的关系文本标签 (位于连线中点上方)
    if (!m_edge.label.isEmpty()) {
        painter->setPen(m_theme.onSurfaceMutedColor);
        QFont font = painter->font();
        font.setPointSize(9);
        painter->setFont(font);
        
        double xMid = (m_edge.startPoint.x() + m_edge.endPoint.x()) / 2.0;
        double yMid = (m_edge.startPoint.y() + m_edge.endPoint.y()) / 2.0;
        
        QRectF labelRect(xMid - 60.0, yMid - 18.0, 120.0, 15.0);
        painter->drawText(labelRect, Qt::AlignCenter, m_edge.label);
    }
    
    painter->restore();
}

void RelationItem::drawRotatedArrow(QPainter *painter, const QPointF &tip, double rad, RenderEdgeKind kind)
{
    painter->save();
    
    // 强制使用实线绘制符号轮廓，避免虚线连线导致符号虚化断裂
    QPen arrowPen = painter->pen();
    arrowPen.setStyle(Qt::SolidLine);
    painter->setPen(arrowPen);
    
    // 平移旋转坐标系，tip 归零作为原点
    painter->translate(tip);
    painter->rotate(rad * 180.0 / 3.141592653589793);
    
    double size = m_theme.arrowSize;
    
    if (kind == RenderEdgeKind::Inheritance || kind == RenderEdgeKind::Realization) {
        // 绘制空心三角形。tip在(0,0)，底边在 x = arrowLen。尖端指向(0,0)，背离终点。
        double arrowLen = size + 2.0;
        QPointF p1(arrowLen, -arrowLen / 2.0);
        QPointF p2(arrowLen, arrowLen / 2.0);
        
        QPainterPath path;
        path.moveTo(0, 0);
        path.lineTo(p1);
        path.lineTo(p2);
        path.closeSubpath();
        
        painter->setBrush(QBrush(m_theme.surfaceColor));
        painter->drawPath(path);
    } 
    else if (kind == RenderEdgeKind::Composition || kind == RenderEdgeKind::Aggregation) {
        // 绘制菱形。tip在(0,0)，后顶点在 x = len。
        double len = size * 1.5;
        QPointF p1(len / 2.0, -size / 2.0);
        QPointF p2(len, 0);
        QPointF p3(len / 2.0, size / 2.0);
        
        QPainterPath path;
        path.moveTo(0, 0);
        path.lineTo(p1);
        path.lineTo(p2);
        path.lineTo(p3);
        path.closeSubpath();
        
        if (kind == RenderEdgeKind::Composition) {
            painter->setBrush(QBrush(painter->pen().color())); // 实心，用连线颜色填充
        } else {
            painter->setBrush(QBrush(m_theme.surfaceColor)); // 空心，用背景色填充
        }
        painter->drawPath(path);
    }
    else if (kind == RenderEdgeKind::Association || kind == RenderEdgeKind::Dependency) {
        // 绘制普通分叉箭头。在终点宿端，尖端在(0,0)，两边往 x 负轴回掠。
        QPointF p1(-size, -size / 2.0);
        QPointF p2(-size, size / 2.0);
        
        painter->drawLine(QPointF(0, 0), p1);
        painter->drawLine(QPointF(0, 0), p2);
    }
    painter->restore();
}

QPainterPath RelationItem::shape() const
{
    QPainterPath path;
    path.moveTo(m_edge.startPoint);
    path.lineTo(m_edge.endPoint);
    
    QPainterPathStroker stroker;
    stroker.setWidth(8.0);
    return stroker.createStroke(path);
}

void RelationItem::setNodes(QGraphicsItem *fromNode, QGraphicsItem *toNode)
{
    m_fromItem = fromNode;
    m_toItem = toNode;
    trackNodes();
}

void RelationItem::trackNodes()
{
    if (!m_fromItem || !m_toItem) {
        return;
    }
    
    prepareGeometryChange();
    
    QRectF rFrom = m_fromItem->sceneBoundingRect();
    QRectF rTo = m_toItem->sceneBoundingRect();
    
    // 计算 from 的 4 个边缘中点
    QPointF fromPoints[4] = {
        QPointF(rFrom.left() + rFrom.width() / 2.0, rFrom.top()),    // Top
        QPointF(rFrom.left() + rFrom.width() / 2.0, rFrom.bottom()), // Bottom
        QPointF(rFrom.left(), rFrom.top() + rFrom.height() / 2.0),   // Left
        QPointF(rFrom.right(), rFrom.top() + rFrom.height() / 2.0)   // Right
    };
    
    // 计算 to 的 4 个边缘中点
    QPointF toPoints[4] = {
        QPointF(rTo.left() + rTo.width() / 2.0, rTo.top()),
        QPointF(rTo.left() + rTo.width() / 2.0, rTo.bottom()),
        QPointF(rTo.left(), rTo.top() + rTo.height() / 2.0),
        QPointF(rTo.right(), rTo.top() + rTo.height() / 2.0)
    };
    
    double minD2 = -1.0;
    QPointF bestFrom, bestTo;
    
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            QPointF diff = fromPoints[i] - toPoints[j];
            double d2 = diff.x() * diff.x() + diff.y() * diff.y();
            if (minD2 < 0.0 || d2 < minD2) {
                minD2 = d2;
                bestFrom = fromPoints[i];
                bestTo = toPoints[j];
            }
        }
    }
    
    m_edge.startPoint = bestFrom;
    m_edge.endPoint = bestTo;
    
    update();
}

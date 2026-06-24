#include "RelationItem.h"
#include "ClassBoxItem.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QGraphicsScene>
#include <QStyleOptionGraphicsItem>
#include <algorithm>
#include <qmath.h>

namespace {
    QPointF sideNormal(BoxSide side)
    {
        switch (side) {
        case BoxSide::Top: return QPointF(0.0, -1.0);
        case BoxSide::Bottom: return QPointF(0.0, 1.0);
        case BoxSide::Left: return QPointF(-1.0, 0.0);
        case BoxSide::Right: return QPointF(1.0, 0.0);
        }
        return QPointF(0.0, 0.0);
    }

    bool isHorizontalSide(BoxSide side)
    {
        return side == BoxSide::Left || side == BoxSide::Right;
    }

    void appendUniquePoint(QVector<QPointF> &points, const QPointF &point)
    {
        if (points.isEmpty() || points.last() != point) {
            points.append(point);
        }
    }

    QPointF rectSidePoint(const QRectF &rect, BoxSide side, double slot)
    {
        slot = qBound(0.0, slot, 1.0);
        switch (side) {
        case BoxSide::Top:
            return QPointF(rect.left() + rect.width() * slot, rect.top());
        case BoxSide::Bottom:
            return QPointF(rect.left() + rect.width() * slot, rect.bottom());
        case BoxSide::Left:
            return QPointF(rect.left(), rect.top() + rect.height() * slot);
        case BoxSide::Right:
            return QPointF(rect.right(), rect.top() + rect.height() * slot);
        }
        return rect.center();
    }

    BoxSide preferredSide(const QRectF &selfRect, const QRectF &otherRect)
    {
        const QPointF selfCenter = selfRect.center();
        const QPointF otherCenter = otherRect.center();
        const double dx = otherCenter.x() - selfCenter.x();
        const double dy = otherCenter.y() - selfCenter.y();
        if (qAbs(dx) >= qAbs(dy)) {
            return dx >= 0.0 ? BoxSide::Right : BoxSide::Left;
        }
        return dy >= 0.0 ? BoxSide::Bottom : BoxSide::Top;
    }

    double sortKeyForSide(const QRectF &otherRect, BoxSide side)
    {
        const QPointF center = otherRect.center();
        switch (side) {
        case BoxSide::Left:
        case BoxSide::Right:
            return center.y();
        case BoxSide::Top:
        case BoxSide::Bottom:
            return center.x();
        }
        return center.x();
    }

    double slotForRelation(const ClassBoxItem *box, const RelationItem *self, BoxSide side)
    {
        if (!box) {
            return 0.5;
        }

        struct SlotCandidate {
            double key = 0.0;
            const RelationItem *relation = nullptr;
        };

    QVector<SlotCandidate> candidates;
    const QRectF selfRect = box->sceneRect();
    for (auto *edgeItem : box->edges()) {
        auto *relation = dynamic_cast<RelationItem*>(edgeItem);
        if (!relation) {
                continue;
            }

            const bool boxIsFrom = relation->fromNodeId() == box->id();
            const QString otherId = boxIsFrom ? relation->toNodeId() : relation->fromNodeId();
            QGraphicsItem *otherGraphics = box->scene() ? nullptr : nullptr;
            if (box->scene()) {
                for (auto *candidate : box->scene()->items()) {
                    if (auto *otherBox = dynamic_cast<ClassBoxItem*>(candidate)) {
                        if (otherBox->id() == otherId) {
                            otherGraphics = otherBox;
                            break;
                        }
                    }
                }
            }
            auto *otherBox = dynamic_cast<ClassBoxItem*>(otherGraphics);
            if (!otherBox) {
                continue;
            }

            const QRectF otherRect = otherBox->sceneRect();
            if (preferredSide(selfRect, otherRect) != side) {
                continue;
            }

            SlotCandidate candidate;
            candidate.key = sortKeyForSide(otherRect, side);
            candidate.relation = relation;
            candidates.append(candidate);
        }

        if (candidates.isEmpty()) {
            return 0.5;
        }

        std::sort(candidates.begin(), candidates.end(), [](const SlotCandidate &a, const SlotCandidate &b) {
            return a.key < b.key;
        });

        for (int i = 0; i < candidates.size(); ++i) {
            if (candidates[i].relation == self) {
                return static_cast<double>(i + 1) / static_cast<double>(candidates.size() + 1);
            }
        }
        return 0.5;
    }

    QVector<QPointF> buildOrthogonalPoints(const QPointF &start, BoxSide fromSide, const QPointF &end, BoxSide toSide)
    {
        const double stubLength = 24.0;
        QVector<QPointF> points;
        const QPointF fromStub = start + sideNormal(fromSide) * stubLength;
        const QPointF toStub = end + sideNormal(toSide) * stubLength;

        appendUniquePoint(points, start);
        appendUniquePoint(points, fromStub);

        if (isHorizontalSide(fromSide) && isHorizontalSide(toSide)) {
            const double midX = (fromStub.x() + toStub.x()) / 2.0;
            appendUniquePoint(points, QPointF(midX, fromStub.y()));
            appendUniquePoint(points, QPointF(midX, toStub.y()));
        } else if (!isHorizontalSide(fromSide) && !isHorizontalSide(toSide)) {
            const double midY = (fromStub.y() + toStub.y()) / 2.0;
            appendUniquePoint(points, QPointF(fromStub.x(), midY));
            appendUniquePoint(points, QPointF(toStub.x(), midY));
        } else if (isHorizontalSide(fromSide) && !isHorizontalSide(toSide)) {
            appendUniquePoint(points, QPointF(toStub.x(), fromStub.y()));
        } else {
            appendUniquePoint(points, QPointF(fromStub.x(), toStub.y()));
        }

        appendUniquePoint(points, toStub);
        appendUniquePoint(points, end);
        return points;
    }

    QVector<QPointF> buildHorizontalGuideRoute(
        const QPointF &start,
        BoxSide fromSide,
        const QPointF &end,
        BoxSide toSide,
        double guideY)
    {
        const double stubLength = 24.0;
        QVector<QPointF> points;
        const QPointF fromStub = start + sideNormal(fromSide) * stubLength;
        const QPointF toStub = end + sideNormal(toSide) * stubLength;

        appendUniquePoint(points, start);
        appendUniquePoint(points, fromStub);
        appendUniquePoint(points, QPointF(fromStub.x(), guideY));
        appendUniquePoint(points, QPointF(toStub.x(), guideY));
        appendUniquePoint(points, toStub);
        appendUniquePoint(points, end);
        return points;
    }

    QVector<QPointF> buildVerticalGuideRoute(
        const QPointF &start,
        BoxSide fromSide,
        const QPointF &end,
        BoxSide toSide,
        double guideX)
    {
        const double stubLength = 24.0;
        QVector<QPointF> points;
        const QPointF fromStub = start + sideNormal(fromSide) * stubLength;
        const QPointF toStub = end + sideNormal(toSide) * stubLength;

        appendUniquePoint(points, start);
        appendUniquePoint(points, fromStub);
        appendUniquePoint(points, QPointF(guideX, fromStub.y()));
        appendUniquePoint(points, QPointF(guideX, toStub.y()));
        appendUniquePoint(points, toStub);
        appendUniquePoint(points, end);
        return points;
    }

    bool segmentHitsRect(const QPointF &a, const QPointF &b, const QRectF &rect)
    {
        if (qFuzzyCompare(a.x(), b.x())) {
            const double x = a.x();
            const double top = qMin(a.y(), b.y());
            const double bottom = qMax(a.y(), b.y());
            return x >= rect.left() && x <= rect.right() && bottom >= rect.top() && top <= rect.bottom();
        }
        if (qFuzzyCompare(a.y(), b.y())) {
            const double y = a.y();
            const double left = qMin(a.x(), b.x());
            const double right = qMax(a.x(), b.x());
            return y >= rect.top() && y <= rect.bottom() && right >= rect.left() && left <= rect.right();
        }
        return QRectF(a, b).normalized().intersects(rect);
    }

    bool routeHitsObstacle(const QVector<QPointF> &points, const QVector<QRectF> &obstacles)
    {
        for (int i = 1; i < points.size(); ++i) {
            const QPointF a = points[i - 1];
            const QPointF b = points[i];
            for (const auto &rect : obstacles) {
                if (segmentHitsRect(a, b, rect)) {
                    return true;
                }
            }
        }
        return false;
    }

    QPainterPath buildPathFromPoints(const QVector<QPointF> &points)
    {
        QPainterPath path;
        if (points.isEmpty()) {
            return path;
        }
        path.moveTo(points.first());
        for (int i = 1; i < points.size(); ++i) {
            path.lineTo(points[i]);
        }
        return path;
    }
}

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
    QRectF rect = m_edge.path.isEmpty()
        ? QRectF(m_edge.startPoint, m_edge.endPoint).normalized()
        : m_edge.path.controlPointRect();
    if (m_edge.hasLabelPosition) {
        rect = rect.united(QRectF(m_edge.labelPosition.x() - 70.0, m_edge.labelPosition.y() - 14.0, 140.0, 24.0));
    }
    return rect.adjusted(-18, -18, 18, 18);
}

void RelationItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    
    bool isHovered = option->state & QStyle::State_MouseOver;
    bool isSelected = option->state & QStyle::State_Selected;
    
    QColor lineColor = (isHovered || isSelected) ? QColor("#4f46e5") : m_theme.primaryColor;
    double penWidth = m_theme.lineWidth;
    Qt::PenStyle penStyle = Qt::SolidLine;
    if (m_edge.kind == RenderEdgeKind::Realization || m_edge.kind == RenderEdgeKind::Dependency) {
        penStyle = Qt::DashLine;
    }

    // 解析自定义线条样式 style 字符串以覆盖默认值
    QStringList parts = m_edge.style.split(QRegularExpression("[;,]"), Qt::SkipEmptyParts);
    for (const auto &part : parts) {
        QString trimmed = part.trimmed();
        if (trimmed.isEmpty()) continue;

        if (trimmed.startsWith("line:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(5).trimmed();
            if (QColor::isValidColorName(col) && !isHovered && !isSelected) {
                lineColor = QColor(col);
            }
        } else if (trimmed.startsWith("color=", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(6).trimmed();
            if (QColor::isValidColorName(col) && !isHovered && !isSelected) {
                lineColor = QColor(col);
            }
        } else if (trimmed.startsWith('#')) {
            if (QColor::isValidColorName(trimmed) && !isHovered && !isSelected) {
                lineColor = QColor(trimmed);
            }
        }

        if (trimmed.startsWith("line.dashed", Qt::CaseInsensitive)) {
            penStyle = Qt::DashLine;
        } else if (trimmed.startsWith("line.dotted", Qt::CaseInsensitive)) {
            penStyle = Qt::DotLine;
        } else if (trimmed == "dashed" || trimmed.contains("dashed", Qt::CaseInsensitive)) {
            penStyle = Qt::DashLine;
        } else if (trimmed == "dotted" || trimmed.contains("dotted", Qt::CaseInsensitive)) {
            penStyle = Qt::DotLine;
        }

        if (trimmed.startsWith("thickness=", Qt::CaseInsensitive)) {
            penWidth = trimmed.mid(10).toDouble();
        } else if (trimmed.startsWith("penwidth=", Qt::CaseInsensitive)) {
            penWidth = trimmed.mid(9).toDouble();
        } else if (trimmed == "bold" || trimmed.contains("bold", Qt::CaseInsensitive)) {
            penWidth *= 2.0;
        }
    }
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    QPen linePen(lineColor, penWidth, penStyle);
    painter->setPen(linePen);
    
    // 2. 根据起止端点几何夹角，计算自适应线身偏移，防止线段穿入三角/菱形图元内部
    QPointF startDirFrom = m_edge.startPoint;
    QPointF startDirTo = m_edge.endPoint;
    QPointF endDirFrom = m_edge.startPoint;
    QPointF endDirTo = m_edge.endPoint;
    if (m_edge.points.size() >= 2) {
        startDirFrom = m_edge.points.first();
        startDirTo = m_edge.points[1];
        endDirFrom = m_edge.points[m_edge.points.size() - 2];
        endDirTo = m_edge.points.last();
    }
    const double startAngle = qAtan2(startDirTo.y() - startDirFrom.y(), startDirTo.x() - startDirFrom.x());
    const double endAngle = qAtan2(endDirTo.y() - endDirFrom.y(), endDirTo.x() - endDirFrom.x());
    
    QPointF startPt = m_edge.startPoint;
    QPointF endPt = m_edge.endPoint;
    
    if (m_edge.kind == RenderEdgeKind::Inheritance || m_edge.kind == RenderEdgeKind::Realization) {
        double arrowLen = m_theme.arrowSize + 2.0;
        startPt = m_edge.startPoint + QPointF(qCos(startAngle), qSin(startAngle)) * arrowLen;
    } else if (m_edge.kind == RenderEdgeKind::Composition || m_edge.kind == RenderEdgeKind::Aggregation) {
        double diamondLen = m_theme.arrowSize * 1.5;
        startPt = m_edge.startPoint + QPointF(qCos(startAngle), qSin(startAngle)) * diamondLen;
    } else if (m_edge.kind == RenderEdgeKind::Nested) {
        double nestedLen = 12.0;
        startPt = m_edge.startPoint + QPointF(qCos(startAngle), qSin(startAngle)) * nestedLen;
    }
    
    // 绘制连线线身
    if (m_edge.path.isEmpty()) {
        painter->drawLine(startPt, endPt);
    } else {
        painter->drawPath(m_edge.path);
    }
    
    // 3. 在对应的端点上绘制矢量旋转对齐的 UML 箭头/符号
    if (m_edge.kind == RenderEdgeKind::Inheritance || 
        m_edge.kind == RenderEdgeKind::Realization || 
        m_edge.kind == RenderEdgeKind::Composition || 
        m_edge.kind == RenderEdgeKind::Aggregation ||
        m_edge.kind == RenderEdgeKind::Nested) {
        // 源端（起点）绘制
        drawRotatedArrow(painter, m_edge.startPoint, startAngle, m_edge.kind);
    } else {
        // 宿端（终点）绘制
        drawRotatedArrow(painter, endDirTo, endAngle, m_edge.kind);
    }
    
    // 4. 绘制关系文本标签 (位于连线中点上方)
    if (!m_edge.label.isEmpty()) {
        painter->setPen(m_theme.onSurfaceMutedColor);
        QFont font = painter->font();
        font.setPointSize(9);
        painter->setFont(font);
        
        double xMid = m_edge.hasLabelPosition ? m_edge.labelPosition.x() : (m_edge.startPoint.x() + m_edge.endPoint.x()) / 2.0;
        double yMid = m_edge.hasLabelPosition ? m_edge.labelPosition.y() : (m_edge.startPoint.y() + m_edge.endPoint.y()) / 2.0;
        
        QRectF labelRect(xMid - 60.0, yMid - 18.0, 120.0, 15.0);
        painter->drawText(labelRect, Qt::AlignCenter, m_edge.label);
    }

    // 5. 绘制多重度限定关联词 (taillabel / headlabel)
    if (!m_edge.taillabel.isEmpty() || !m_edge.headlabel.isEmpty()) {
        painter->save();
        painter->setPen(m_theme.onSurfaceMutedColor);
        QFont font = painter->font();
        font.setPointSize(8);
        painter->setFont(font);
        
        // taillabel (起点端)
        if (!m_edge.taillabel.isEmpty() && m_edge.points.size() >= 2) {
            QPointF p0 = m_edge.points.first();
            QPointF p1 = m_edge.points[1];
            QPointF v = p1 - p0;
            double len = qSqrt(v.x() * v.x() + v.y() * v.y());
            if (len > 0) {
                QPointF dir = v / len;
                QPointF normal(-dir.y(), dir.x());
                QPointF pos = p0 + dir * 25.0 + normal * 12.0;
                QRectF textRect(pos.x() - 40.0, pos.y() - 10.0, 80.0, 20.0);
                painter->drawText(textRect, Qt::AlignCenter, m_edge.taillabel);
            }
        }

        // headlabel (终点端)
        if (!m_edge.headlabel.isEmpty() && m_edge.points.size() >= 2) {
            QPointF pN = m_edge.points.last();
            QPointF pN_1 = m_edge.points[m_edge.points.size() - 2];
            QPointF v = pN - pN_1;
            double len = qSqrt(v.x() * v.x() + v.y() * v.y());
            if (len > 0) {
                QPointF dir = v / len;
                QPointF normal(-dir.y(), dir.x());
                QPointF pos = pN - dir * 25.0 + normal * 12.0;
                QRectF textRect(pos.x() - 40.0, pos.y() - 10.0, 80.0, 20.0);
                painter->drawText(textRect, Qt::AlignCenter, m_edge.headlabel);
            }
        }
        painter->restore();
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
    else if (kind == RenderEdgeKind::Nested) {
        // 绘制嵌套关系特殊符号：直径大约为 12 像素的空心圆圈，加上正中心 '+' 符号
        double radius = 6.0;
        QRectF circleRect(0.0, -radius, radius * 2.0, radius * 2.0);
        
        // 空心圆圈，背景色填充
        painter->setBrush(QBrush(m_theme.surfaceColor));
        painter->drawEllipse(circleRect);
        
        // 绘制加号
        // 水平线：从 (3, 0) 到 (9, 0)
        painter->drawLine(QPointF(radius - 3.0, 0), QPointF(radius + 3.0, 0));
        // 垂直线：从 (6, -3) 到 (6, 3)
        painter->drawLine(QPointF(radius, -3.0), QPointF(radius, 3.0));
    }
    painter->restore();
}

QPainterPath RelationItem::shape() const
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

void RelationItem::setNodes(QGraphicsItem *fromNode, QGraphicsItem *toNode)
{
    m_fromItem = fromNode;
    m_toItem = toNode;
}


void RelationItem::trackNodes()
{
    if (!m_fromItem || !m_toItem) {
        return;
    }
    
    prepareGeometryChange();
    
    QRectF rFrom = m_fromItem->sceneBoundingRect();
    QRectF rTo = m_toItem->sceneBoundingRect();

    const BoxSide fromSide = preferredSide(rFrom, rTo);
    const BoxSide toSide = preferredSide(rTo, rFrom);
    const auto *fromBox = dynamic_cast<const ClassBoxItem*>(m_fromItem);
    const auto *toBox = dynamic_cast<const ClassBoxItem*>(m_toItem);
    const double fromSlot = slotForRelation(fromBox, this, fromSide);
    const double toSlot = slotForRelation(toBox, this, toSide);

    QPointF bestFrom = rectSidePoint(rFrom, fromSide, fromSlot);
    QPointF bestTo = rectSidePoint(rTo, toSide, toSlot);

    // 核心重构：将复杂的端口吸附点计算完全委派给 ClassBoxItem，实现高内聚
    if (fromBox) {
        bestFrom = fromBox->portPoint(m_edge.fromPort, bestFrom, fromSide);
    }
    if (toBox) {
        bestTo = toBox->portPoint(m_edge.toPort, bestTo, toSide);
    }
    
    m_edge.startPoint = bestFrom;
    m_edge.endPoint = bestTo;

    QVector<QRectF> obstacles;
    QRectF extent = rFrom.united(rTo);
    if (scene()) {
        for (auto *item : scene()->items()) {
            if (item == m_fromItem || item == m_toItem) {
                continue;
            }
            if (auto *box = dynamic_cast<ClassBoxItem*>(item)) {
                QRectF rect = box->sceneRect().adjusted(-10.0, -10.0, 10.0, 10.0);
                obstacles.append(rect);
                extent = extent.united(rect);
            }
        }
    }

    QVector<QVector<QPointF>> candidates;
    candidates.append(buildOrthogonalPoints(bestFrom, fromSide, bestTo, toSide));
    candidates.append(buildHorizontalGuideRoute(bestFrom, fromSide, bestTo, toSide, extent.top() - 30.0));
    candidates.append(buildHorizontalGuideRoute(bestFrom, fromSide, bestTo, toSide, extent.bottom() + 30.0));
    candidates.append(buildVerticalGuideRoute(bestFrom, fromSide, bestTo, toSide, extent.left() - 30.0));
    candidates.append(buildVerticalGuideRoute(bestFrom, fromSide, bestTo, toSide, extent.right() + 30.0));

    m_edge.points = candidates.first();
    for (const auto &candidate : candidates) {
        if (!routeHitsObstacle(candidate, obstacles)) {
            m_edge.points = candidate;
            break;
        }
    }
    m_edge.path = buildPathFromPoints(m_edge.points);

    if (!m_edge.label.isEmpty()) {
        const int labelIndex = qMax(0, (m_edge.points.size() - 2) / 2);
        const QPointF a = m_edge.points[labelIndex];
        const QPointF b = m_edge.points[qMin(labelIndex + 1, m_edge.points.size() - 1)];
        m_edge.labelPosition = QPointF((a.x() + b.x()) / 2.0, (a.y() + b.y()) / 2.0 - 12.0);
        m_edge.hasLabelPosition = true;
    } else {
        m_edge.hasLabelPosition = false;
    }
    
    update();
}

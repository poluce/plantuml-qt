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
        
        // 如果点数正好符合 B样条控制点数 3n + 1 (且大于 1)
        if (points.size() > 1 && (points.size() - 1) % 3 == 0) {
            path.moveTo(points.first());
            int i = 1;
            for (; i + 2 < points.size(); i += 3) {
                path.cubicTo(points[i], points[i + 1], points[i + 2]);
            }
            return path;
        }
        
        // 否则，说明这是拖动时生成的普通折线点。
        // 使用二次贝塞尔曲线在拐角处做圆润化平滑
        path.moveTo(points.first());
        double cornerRadius = 15.0; // 圆角半径
        
        for (int i = 1; i < points.size() - 1; ++i) {
            QPointF p_prev = points[i - 1];
            QPointF p_curr = points[i];
            QPointF p_next = points[i + 1];
            
            // 计算向量
            QPointF v1 = p_prev - p_curr;
            QPointF v2 = p_next - p_curr;
            
            double len1 = qSqrt(v1.x() * v1.x() + v1.y() * v1.y());
            double len2 = qSqrt(v2.x() * v2.x() + v2.y() * v2.y());
            
            if (len1 > 0 && len2 > 0) {
                double r = qMin(cornerRadius, qMin(len1 / 2.0, len2 / 2.0));
                QPointF p_a = p_curr + (v1 / len1) * r;
                QPointF p_b = p_curr + (v2 / len2) * r;
                
                path.lineTo(p_a);
                path.quadTo(p_curr, p_b);
            } else {
                path.lineTo(p_curr);
            }
        }
        path.lineTo(points.last());
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
    , m_initialPoints(edge.points)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    resolveStyle();
}

void RelationItem::resolveStyle()
{
    m_resolvedColor = m_theme.primaryColor;
    m_resolvedPenWidth = m_theme.lineWidth;
    m_resolvedPenStyle = Qt::SolidLine;
    if (m_edge.kind == RenderEdgeKind::Realization || m_edge.kind == RenderEdgeKind::Dependency) {
        m_resolvedPenStyle = Qt::DashLine;
    }

    if (m_edge.style.isEmpty()) return;

    QStringList parts = m_edge.style.split(QRegularExpression("[;,]"), Qt::SkipEmptyParts);
    for (const auto &part : parts) {
        QString trimmed = part.trimmed();
        if (trimmed.isEmpty()) continue;

        if (trimmed.startsWith("line:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(5).trimmed();
            if (QColor::isValidColorName(col)) m_resolvedColor = QColor(col);
        } else if (trimmed.startsWith("color=", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(6).trimmed();
            if (QColor::isValidColorName(col)) m_resolvedColor = QColor(col);
        } else if (trimmed.startsWith('#')) {
            if (QColor::isValidColorName(trimmed)) m_resolvedColor = QColor(trimmed);
        }

        if (trimmed.contains("dashed", Qt::CaseInsensitive) || trimmed.contains("line.dashed", Qt::CaseInsensitive)) {
            m_resolvedPenStyle = Qt::DashLine;
        } else if (trimmed.contains("dotted", Qt::CaseInsensitive) || trimmed.contains("line.dotted", Qt::CaseInsensitive)) {
            m_resolvedPenStyle = Qt::DotLine;
        } else if (trimmed.contains("solid", Qt::CaseInsensitive)) {
            m_resolvedPenStyle = Qt::SolidLine;
        }

        if (trimmed.startsWith("thickness=", Qt::CaseInsensitive)) {
            m_resolvedPenWidth = trimmed.mid(10).toDouble();
        } else if (trimmed.startsWith("penwidth=", Qt::CaseInsensitive)) {
            m_resolvedPenWidth = trimmed.mid(9).toDouble();
        } else if (trimmed.contains("bold", Qt::CaseInsensitive)) {
            m_resolvedPenWidth *= 2.0;
        }
    }
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
    
    QColor lineColor = (isHovered || isSelected) ? QColor("#4f46e5") : m_resolvedColor;
    double penWidth = (isHovered || isSelected) ? m_resolvedPenWidth + 1.0 : m_resolvedPenWidth;
    Qt::PenStyle penStyle = m_resolvedPenStyle;
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    QPen linePen(lineColor, penWidth, penStyle);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(linePen);
    
    // 1. 根据起止端点几何夹角，计算自适应线身偏移，防止线段穿入三角/菱形图元内部
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
    
    // 2. 线身路径缩进 (相邻控制点平移优化)
    QVector<QPointF> drawPoints = m_edge.points;
    if (drawPoints.size() >= 2) {
        QPointF p0 = drawPoints.first();
        QPointF p1 = drawPoints[1];
        QPointF vStart = p1 - p0;
        double lenStart = qSqrt(vStart.x() * vStart.x() + vStart.y() * vStart.y());
        
        QPointF pN = drawPoints.last();
        QPointF pN_1 = drawPoints[drawPoints.size() - 2];
        QPointF vEnd = pN - pN_1;
        double lenEnd = qSqrt(vEnd.x() * vEnd.x() + vEnd.y() * vEnd.y());
        
        double L_start = 0.0;
        double L_end = 0.0;
        
        if (m_edge.kind == RenderEdgeKind::Inheritance || m_edge.kind == RenderEdgeKind::Realization) {
            L_start = m_theme.arrowSize + 2.0;
        } else if (m_edge.kind == RenderEdgeKind::Composition || m_edge.kind == RenderEdgeKind::Aggregation) {
            L_start = m_theme.arrowSize * 1.5;
        } else if (m_edge.kind == RenderEdgeKind::Nested) {
            L_start = 12.0;
        } else if (m_edge.kind == RenderEdgeKind::Square) {
            L_start = m_theme.arrowSize;
        } else if (m_edge.kind == RenderEdgeKind::Crowfoot || m_edge.kind == RenderEdgeKind::Hat) {
            L_start = m_theme.arrowSize;
        } else if (m_edge.kind == RenderEdgeKind::Cross) {
            L_start = 6.0;
        }
        
        if (m_edge.kind == RenderEdgeKind::Association || m_edge.kind == RenderEdgeKind::Dependency) {
            L_end = 1.0;
        }
        
        if (lenStart > 0 && L_start > 0) {
            QPointF T_start = vStart / lenStart;
            QPointF deltaStart = T_start * L_start;
            drawPoints[0] += deltaStart;
            // 物理校准相邻点，保持 G1 切向连续性
            drawPoints[1] += deltaStart;
        }
        
        if (lenEnd > 0 && L_end > 0) {
            QPointF T_end = vEnd / lenEnd;
            QPointF deltaEnd = -T_end * L_end;
            drawPoints[drawPoints.size() - 1] += deltaEnd;
            drawPoints[drawPoints.size() - 2] += deltaEnd;
        }
    }
    
    QPainterPath drawPath = buildPathFromPoints(drawPoints);
    painter->drawPath(drawPath);
    
    // 3. 绘制矢量旋转对齐的 UML 箭头/符号
    if (m_edge.kind == RenderEdgeKind::Inheritance || 
        m_edge.kind == RenderEdgeKind::Realization || 
        m_edge.kind == RenderEdgeKind::Composition || 
        m_edge.kind == RenderEdgeKind::Aggregation ||
        m_edge.kind == RenderEdgeKind::Nested ||
        m_edge.kind == RenderEdgeKind::Square ||
        m_edge.kind == RenderEdgeKind::Cross ||
        m_edge.kind == RenderEdgeKind::Crowfoot ||
        m_edge.kind == RenderEdgeKind::Hat) {
        drawRotatedArrow(painter, m_edge.startPoint, startAngle, m_edge.kind);
    } else if (m_edge.kind != RenderEdgeKind::AssociationLine) {
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

    // 5. 绘制多重度限定关联词并处理碰撞反转退避
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
                QPointF T = v / len;
                QPointF N(-T.y(), T.x());
                
                double d_offset = 24.0;
                double n_offset = 12.0;
                
                QFontMetrics fm(painter->font());
                int w = fm.horizontalAdvance(m_edge.taillabel);
                int h = fm.height();
                
                QPointF pos = p0 + T * d_offset + N * n_offset;
                QRectF textRect(pos.x() - w / 2.0, pos.y() - h / 2.0, w, h);
                
                if (m_fromItem) {
                    QRectF fromRect = m_fromItem->sceneBoundingRect();
                    if (auto *box = dynamic_cast<ClassBoxItem*>(m_fromItem)) {
                        if (box->metaType() == "diamond" || box->metaType() == "<>" ||
                            box->metaType() == "circle" || box->metaType() == "()") {
                            fromRect = QRectF(p0.x() - 15.0, p0.y() - 15.0, 30.0, 30.0);
                        }
                    }
                    if (textRect.intersects(fromRect)) {
                        pos = p0 + T * d_offset - N * n_offset;
                        textRect = QRectF(pos.x() - w / 2.0, pos.y() - h / 2.0, w, h);
                        
                        int attempts = 0;
                        while (textRect.intersects(fromRect) && attempts < 5) {
                            d_offset += 15.0;
                            pos = p0 + T * d_offset - N * n_offset;
                            textRect = QRectF(pos.x() - w / 2.0, pos.y() - h / 2.0, w, h);
                            attempts++;
                        }
                    }
                }
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
                QPointF T = v / len;
                QPointF N(-T.y(), T.x());
                
                double d_offset = 24.0;
                double n_offset = 12.0;
                
                QFontMetrics fm(painter->font());
                int w = fm.horizontalAdvance(m_edge.headlabel);
                int h = fm.height();
                
                QPointF pos = pN - T * d_offset + N * n_offset;
                QRectF textRect(pos.x() - w / 2.0, pos.y() - h / 2.0, w, h);
                
                if (m_toItem) {
                    QRectF toRect = m_toItem->sceneBoundingRect();
                    if (auto *box = dynamic_cast<ClassBoxItem*>(m_toItem)) {
                        if (box->metaType() == "diamond" || box->metaType() == "<>" ||
                            box->metaType() == "circle" || box->metaType() == "()") {
                            toRect = QRectF(pN.x() - 15.0, pN.y() - 15.0, 30.0, 30.0);
                        }
                    }
                    if (textRect.intersects(toRect)) {
                        pos = pN - T * d_offset - N * n_offset;
                        textRect = QRectF(pos.x() - w / 2.0, pos.y() - h / 2.0, w, h);
                        
                        int attempts = 0;
                        while (textRect.intersects(toRect) && attempts < 5) {
                            d_offset += 15.0;
                            pos = pN - T * d_offset - N * n_offset;
                            textRect = QRectF(pos.x() - w / 2.0, pos.y() - h / 2.0, w, h);
                            attempts++;
                        }
                    }
                }
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
        
        painter->setBrush(Qt::NoBrush);
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
        painter->drawLine(QPointF(radius - 3.0, 0), QPointF(radius + 3.0, 0));
        painter->drawLine(QPointF(radius, -3.0), QPointF(radius, 3.0));
    }
    else if (kind == RenderEdgeKind::Square) {
        // 绘制方形。尖端在(0,0)，中心在 x = size/2，底边在 x = size。
        QRectF rect(0.0, -size / 2.0, size, size);
        painter->setBrush(QBrush(m_theme.surfaceColor));
        painter->drawRect(rect);
    }
    else if (kind == RenderEdgeKind::Cross) {
        // 绘制交叉叉号 X。
        double offset = size / 2.0;
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(0, -offset), QPointF(size, offset));
        painter->drawLine(QPointF(0, offset), QPointF(size, -offset));
    }
    else if (kind == RenderEdgeKind::Crowfoot) {
        // 绘制三叉鸟爪 (Crowfoot)。
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(0, 0), QPointF(size, -size / 2.0));
        painter->drawLine(QPointF(0, 0), QPointF(size, size / 2.0));
        painter->drawLine(QPointF(size, -size / 2.0), QPointF(size, size / 2.0));
    }
    else if (kind == RenderEdgeKind::Hat) {
        // 绘制尖括号 (Hat)。
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(0, 0), QPointF(size, -size / 2.0));
        painter->drawLine(QPointF(0, 0), QPointF(size, size / 2.0));
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
    
    // 如果起止某一端是虚拟关联中点，首先更新该虚拟点的位置
    if (auto *vFrom = dynamic_cast<ClassBoxItem*>(m_fromItem)) {
        if (vFrom->node().metaType == "point") {
            vFrom->updateAssocPosition();
        }
    }
    if (auto *vTo = dynamic_cast<ClassBoxItem*>(m_toItem)) {
        if (vTo->node().metaType == "point") {
            vTo->updateAssocPosition();
        }
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

    // 如果原始连线包含 Graphviz 算出来的 B样条曲线控制点，采用高精曲线形变拉伸跟随，保留精美曲线风格
    if (m_initialPoints.size() > 2) {
        QPointF U = m_initialEnd - m_initialStart;
        double len2_old = U.x() * U.x() + U.y() * U.y();
        if (len2_old > 0.0) {
            QPointF V = bestTo - bestFrom;
            QPointF N_old(-U.y(), U.x());
            double len2_N = N_old.x() * N_old.x() + N_old.y() * N_old.y();
            QPointF N_new(-V.y(), V.x());
            
            QVector<QPointF> newPoints;
            for (const auto &pOld : m_initialPoints) {
                double t = ((pOld.x() - m_initialStart.x()) * U.x() + (pOld.y() - m_initialStart.y()) * U.y()) / len2_old;
                double h = 0.0;
                if (len2_N > 0.0) {
                    h = ((pOld.x() - m_initialStart.x()) * N_old.x() + (pOld.y() - m_initialStart.y()) * N_old.y()) / len2_N;
                }
                QPointF pNew = bestFrom + t * V + h * N_new;
                newPoints.append(pNew);
            }
            m_edge.points = newPoints;
            m_edge.path = buildPathFromPoints(m_edge.points);
            
            // 更新关系文本标签位置 (置于新曲线中段控制点上方)
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
            return; // 成功应用曲线拉伸，直接返回！
        }
    }

    QVector<QRectF> obstacles;
    QRectF extent = rFrom.united(rTo);
    if (scene()) {
        for (auto *item : scene()->items()) {
            if (item == m_fromItem || item == m_toItem) {
                continue;
            }
            if (auto *box = dynamic_cast<ClassBoxItem*>(item)) {
                if (box->node().metaType == "point") {
                    continue; // 虚拟交汇中点无实体，绝对不作为连线的障碍物避让，防碍拖拽随动时坐标重算错乱
                }
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

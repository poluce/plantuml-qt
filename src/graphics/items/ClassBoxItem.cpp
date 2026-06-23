#include "ClassBoxItem.h"
#include "RelationItem.h"
#include "NoteLineItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ClassBoxItem::ClassBoxItem(const RenderNode &node, const RenderTheme &theme)
    : m_node(node)
    , m_theme(theme)
    , m_initialPos(node.rect.topLeft())
    , m_fillColor(theme.surfaceColor)
    , m_borderColor(theme.primaryColor)
    , m_penWidth(theme.lineWidth)
    , m_penStyle(Qt::SolidLine)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
    // 解析 style 字符串属性
    QStringList parts = node.style.split(QRegularExpression("[;,]"), Qt::SkipEmptyParts);
    for (const auto &part : parts) {
        QString trimmed = part.trimmed();
        if (trimmed.isEmpty()) continue;
        
        if (trimmed.startsWith("back:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(5).trimmed();
            if (QColor::isValidColorName(col)) m_fillColor = QColor(col);
        } else if (trimmed.startsWith("fill=", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(5).trimmed();
            if (QColor::isValidColorName(col)) m_fillColor = QColor(col);
        } else if (trimmed.startsWith("fillcolor=", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(10).trimmed();
            if (QColor::isValidColorName(col)) m_fillColor = QColor(col);
        } else if (trimmed.startsWith('#')) {
            if (QColor::isValidColorName(trimmed)) m_fillColor = QColor(trimmed);
        }

        if (trimmed.startsWith("border.color:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(13).trimmed();
            if (QColor::isValidColorName(col)) m_borderColor = QColor(col);
        } else if (trimmed.startsWith("color=", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(6).trimmed();
            if (QColor::isValidColorName(col)) m_borderColor = QColor(col);
        } else if (trimmed.startsWith("line:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(5).trimmed();
            if (QColor::isValidColorName(col)) m_borderColor = QColor(col);
        } else if (trimmed.startsWith("border:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(7).trimmed();
            if (QColor::isValidColorName(col)) m_borderColor = QColor(col);
        }

        if (trimmed.startsWith("penwidth=", Qt::CaseInsensitive)) {
            m_penWidth = trimmed.mid(9).toDouble();
        } else if (trimmed == "bold" || trimmed.contains("bold", Qt::CaseInsensitive)) {
            m_penWidth = theme.lineWidth * 2.0;
        }

        if (trimmed.startsWith("border.style:", Qt::CaseInsensitive)) {
            QString styleStr = trimmed.mid(13).trimmed().toLower();
            if (styleStr == "dashed") m_penStyle = Qt::DashLine;
            else if (styleStr == "dotted") m_penStyle = Qt::DotLine;
        } else if (trimmed == "dashed" || trimmed.contains("dashed", Qt::CaseInsensitive)) {
            m_penStyle = Qt::DashLine;
        } else if (trimmed == "dotted" || trimmed.contains("dotted", Qt::CaseInsensitive)) {
            m_penStyle = Qt::DotLine;
        }
    }

    // 将卡片的绝对坐标转换为局部坐标绘制，使用 setPos 决定其场景空间位置，实现拖拽自适应
    setPos(node.rect.topLeft());
    m_node.rect = QRectF(0, 0, node.rect.width(), node.rect.height());
}

QRectF ClassBoxItem::boundingRect() const
{
    const QRectF &rect = m_node.rect;
    
    // 留出 6 像素的安全包围描边余量
    return QRectF(rect.x() - 6, rect.y() - 6, rect.width() + 12, rect.height() + 12);
}

void ClassBoxItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    
    const QRectF &rect = m_node.rect;
    bool isHovered = option->state & QStyle::State_MouseOver;
    bool isSelected = option->state & QStyle::State_Selected;
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    // 1. 如果选中，在卡片外部画一圈淡雅的靛蓝发光外环
    if (isSelected) {
        QPen selectPen(QColor(99, 102, 241, 120), 4.0);
        painter->setPen(selectPen);
        painter->setBrush(Qt::NoBrush);
        
        if (m_node.metaType == "circle" || m_node.metaType == "()") {
            QPointF center = rect.center();
            painter->drawEllipse(QRectF(center.x() - 17.0, center.y() - 17.0 - 10.0, 34.0, 34.0));
        } else if (m_node.metaType == "diamond" || m_node.metaType == "<>") {
            QPointF center(rect.center().x(), rect.center().y() - 10.0);
            QPolygonF selPoly;
            selPoly << QPointF(center.x(), center.y() - 14.0)
                    << QPointF(center.x() + 14.0, center.y())
                    << QPointF(center.x(), center.y() + 14.0)
                    << QPointF(center.x() - 14.0, center.y());
            painter->drawPolygon(selPoly);
        } else {
            painter->drawRoundedRect(rect.adjusted(-2, -2, 2, 2), m_theme.nodeRadius + 1, m_theme.nodeRadius + 1);
        }
    }
    
    // 2. 根据不同的元数据类型进行分块绘制
    if (m_node.kind == RenderNodeKind::Note) {
        // ================= Note 气泡框绘制 =================
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        // 设置 Note 风格色彩（经典淡黄色与琥珀色边框）
        QColor noteBg = QColor("#fffbeb"); // 极浅黄背景
        QColor noteBorder = QColor("#d97706"); // 琥珀色边框
        if (isHovered) {
            noteBg = QColor("#fef3c7");
        }

        if (m_fillColor != m_theme.surfaceColor) {
            noteBg = m_fillColor;
        }
        if (m_borderColor != m_theme.primaryColor) {
            noteBorder = m_borderColor;
        }

        painter->setPen(QPen(noteBorder, m_penWidth, m_penStyle));
        painter->setBrush(QBrush(noteBg));

        // 绘制折角边框路径
        QPainterPath path;
        const double fold = 10.0;
        path.moveTo(rect.left(), rect.top());
        path.lineTo(rect.right() - fold, rect.top());
        path.lineTo(rect.right(), rect.top() + fold);
        path.lineTo(rect.right(), rect.bottom());
        path.lineTo(rect.left(), rect.bottom());
        path.closeSubpath();
        painter->drawPath(path);

        // 绘制折折角背面小三角
        QPainterPath foldPath;
        foldPath.moveTo(rect.right() - fold, rect.top());
        foldPath.lineTo(rect.right() - fold, rect.top() + fold);
        foldPath.lineTo(rect.right(), rect.top() + fold);
        painter->setBrush(QBrush(noteBg.darker(115))); // 比背景稍暗的折角阴影
        painter->drawPath(foldPath);

        // 绘制 Note 文本
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(false);
        painter->setFont(font);

        // 预留内衬边距并支持自动换行
        QRectF textRect = rect.adjusted(10, 8, -10, -8);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_node.displayName);

        painter->restore();
    }
    else if (m_node.metaType == "json") {
        // ================= JSON 卡片绘制 =================
        // 画统一底色的圆角矩形
        painter->setPen(QPen(m_borderColor, m_penWidth, m_penStyle));
        painter->setBrush(QBrush(m_fillColor));
        painter->drawRoundedRect(rect, m_theme.nodeRadius, m_theme.nodeRadius);

        // 绘制标题
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(10);
        font.setBold(true);
        painter->setFont(font);

        double yOff = rect.y() + 12.0;
        QRectF titleRect(rect.x() + 10.0, yOff, rect.width() - 20.0, 20.0);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
        yOff += 22.0;

        // 绘制成员列表
        font.setBold(false);
        font.setPointSize(9);
        painter->setFont(font);
        painter->setPen(m_theme.onSurfaceMutedColor);

        double memberLineHeight = 18.0;
        for (const auto &member : m_node.members) {
            QRectF labelRect(rect.x() + 10.0, yOff, rect.width() - 20.0, memberLineHeight);
            painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, member.rawText);
            yOff += memberLineHeight;
        }
    }
    else if (m_node.metaType == "circle" || m_node.metaType == "()") {
        // ================= 圆圈棒棒糖绘制 =================
        QPointF center(rect.center().x(), rect.center().y() - 10.0);
        QRectF circleRect(center.x() - 15.0, center.y() - 15.0, 30.0, 30.0);

        painter->setPen(QPen(m_borderColor, m_penWidth, m_penStyle));
        painter->setBrush(QBrush(m_fillColor));
        painter->drawEllipse(circleRect);

        // 文字绘制
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(true);
        painter->setFont(font);

        QRectF textRect(rect.x(), center.y() + 18.0, rect.width(), 20.0);
        painter->drawText(textRect, Qt::AlignCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
    }
    else if (m_node.metaType == "diamond" || m_node.metaType == "<>") {
        // ================= 菱形绘制 =================
        QPointF center(rect.center().x(), rect.center().y() - 10.0);
        QPolygonF polygon;
        polygon << QPointF(center.x(), center.y() - 12.0)
                << QPointF(center.x() + 12.0, center.y())
                << QPointF(center.x(), center.y() + 12.0)
                << QPointF(center.x() - 12.0, center.y());

        painter->setPen(QPen(m_borderColor, m_penWidth, m_penStyle));
        painter->setBrush(QBrush(m_fillColor));
        painter->drawPolygon(polygon);

        // 文字绘制
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(true);
        painter->setFont(font);

        QRectF textRect(rect.x(), center.y() + 16.0, rect.width(), 20.0);
        painter->drawText(textRect, Qt::AlignCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
    }
    else {
        // ================= 普通类节点卡片绘制 =================
        const double headerHeight = 35.0;
        
        // 2. 绘制类卡片大背景
        QPen borderPen(m_borderColor, m_penWidth, m_penStyle);
        painter->setPen(borderPen);
        painter->setBrush(QBrush(m_fillColor));
        painter->drawRoundedRect(rect, m_theme.nodeRadius, m_theme.nodeRadius);
        
        // 3. 绘制上半部的类名头部背景区
        QColor headerFill = isHovered ? QColor("#c7d2fe") : m_theme.primaryLightColor;
        if (m_fillColor != m_theme.surfaceColor) {
            headerFill = m_fillColor.darker(110);
        }
        painter->setBrush(QBrush(headerFill));
        
        QPainterPath headerPath;
        QRectF headRect(rect.x(), rect.y(), rect.width(), headerHeight);
        headerPath.addRoundedRect(headRect, m_theme.nodeRadius, m_theme.nodeRadius);
        
        QRectF clipRect(rect.x(), rect.y() + headerHeight - 8.0, rect.width(), 8.0);
        QPainterPath clipPath;
        clipPath.addRect(clipRect);
        headerPath = headerPath.united(clipPath);
        
        painter->drawPath(headerPath);
        
        // 4. 绘制类名区与成员区分割实线
        painter->setPen(QPen(m_theme.borderColor, m_theme.lineWidth));
        painter->drawLine(QPointF(rect.x(), rect.y() + headerHeight), QPointF(rect.x() + rect.width(), rect.y() + headerHeight));
        
        // 5. 绘制类名与元类/Stereotype修饰文本
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(10);
        font.setBold(true);
        painter->setFont(font);
        
        bool hasTopLabel = !m_node.stereotype.isEmpty() || (m_node.metaType == "interface" || m_node.metaType == "enum");
        if (hasTopLabel) {
            QString topText;
            if (!m_node.stereotype.isEmpty()) {
                topText = m_node.stereotype;
                if (!topText.startsWith("<<")) topText = "<<" + topText;
                if (!topText.endsWith(">>")) topText = topText + ">>";
            } else {
                topText = (m_node.metaType == "interface") ? "<<interface>>" : "<<enumeration>>";
            }
            
            QFont metaFont = font;
            metaFont.setPointSize(8);
            metaFont.setBold(false);
            metaFont.setItalic(true);
            painter->setFont(metaFont);
            
            QRectF metaRect(rect.x(), rect.y() + 3, rect.width(), 14);
            painter->drawText(metaRect, Qt::AlignCenter, topText);
            
            painter->setFont(font);
            QRectF nameRect(rect.x(), rect.y() + 15, rect.width(), 18);
            painter->drawText(nameRect, Qt::AlignCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
        } else {
            painter->drawText(headRect, Qt::AlignCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
        }
        
        // 6. 依次换行左对齐绘制类成员 (属性与方法)
        font.setPointSize(9);
        font.setBold(false);
        painter->setFont(font);
        painter->setPen(m_theme.onSurfaceMutedColor);
        
        double memberLineHeight = 18.0;
        for (int k = 0; k < m_node.members.size(); ++k) {
            double yOff = rect.y() + headerHeight + 6.0 + k * memberLineHeight;
            const auto &member = m_node.members[k];
            
            if (member.isSeparator) {
                painter->save();
                QPen sepPen(m_theme.borderColor, 1);
                if (member.rawText.contains('.')) {
                    sepPen.setStyle(Qt::DashLine);
                } else if (member.rawText.contains('=')) {
                    sepPen.setStyle(Qt::SolidLine);
                } else {
                    sepPen.setStyle(Qt::SolidLine);
                }
                painter->setPen(sepPen);
                double lineY = yOff + memberLineHeight / 2.0;
                painter->drawLine(rect.x() + 4.0, lineY, rect.x() + rect.width() - 4.0, lineY);
                
                if (!member.separatorText.isEmpty()) {
                    QFont sepFont = painter->font();
                    sepFont.setPointSize(8);
                    sepFont.setItalic(true);
                    painter->setFont(sepFont);
                    painter->setPen(m_theme.onSurfaceMutedColor);
                    
                    QFontMetrics fm(sepFont);
                    int textW = fm.horizontalAdvance(member.separatorText) + 12;
                    QRectF labelRect(rect.x() + (rect.width() - textW) / 2.0, yOff, textW, memberLineHeight);
                    painter->fillRect(labelRect, m_theme.surfaceColor);
                    painter->drawText(labelRect, Qt::AlignCenter, member.separatorText);
                }
                painter->restore();
            } else {
                QRectF labelRect(rect.x() + 10.0, yOff, rect.width() - 20.0, memberLineHeight);
                painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, member.rawText);
            }
        }
    }
    
    painter->restore();
}

void ClassBoxItem::addEdge(QGraphicsItem *edge)
{
    if (!m_edges.contains(edge)) {
        m_edges.append(edge);
    }
}

void ClassBoxItem::updateEdges()
{
    for (auto *edge : m_edges) {
        if (auto *rel = dynamic_cast<RelationItem*>(edge)) {
            rel->trackNodes();
        } else if (auto *noteLine = dynamic_cast<NoteLineItem*>(edge)) {
            noteLine->trackNodes();
        }
    }
}

QVariant ClassBoxItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        updateEdges();
    }
    return QGraphicsItem::itemChange(change, value);
}

QPointF ClassBoxItem::portPoint(const QString &portId, const QPointF &fallback, BoxSide side) const
{
    // 1. 如果是 Note 节点，Y 轴强制对其侧边中心，X 轴物理贴边
    if (m_node.kind == RenderNodeKind::Note) {
        QRectF r = mapRectToScene(m_node.rect);
        double sceneY = mapToScene(QPointF(0, m_node.rect.height() / 2.0)).y();
        if (side == BoxSide::Left) {
            return QPointF(r.left(), sceneY);
        } else if (side == BoxSide::Right) {
            return QPointF(r.right(), sceneY);
        }
        return fallback;
    }

    // 2. 如果是 Class 且无指定端口，保持默认贴边 X 坐标，Y 轴由 preferred 决定
    if (portId.isEmpty() || !portId.startsWith("member_")) {
        QRectF r = mapRectToScene(m_node.rect);
        if (side == BoxSide::Left) {
            return QPointF(r.left(), fallback.y());
        } else if (side == BoxSide::Right) {
            return QPointF(r.right(), fallback.y());
        }
        return fallback;
    }

    // 3. 如果是 Class 且指定了特定的成员端口：Y 轴对准成员文字中线，X 轴物理贴边
    bool ok = false;
    int memberIdx = portId.mid(7).toInt(&ok);
    if (!ok || memberIdx < 0 || memberIdx >= m_node.members.size()) {
        return fallback;
    }
    
    // 使用不含描边外扩余量的物理 rect 计算场景坐标边缘，保证连线物理贴合
    QRectF r = mapRectToScene(m_node.rect);
    const double headerHeight = 35.0;
    const double memberLineHeight = 18.0;
    double localY = headerHeight + 6.0 + memberIdx * memberLineHeight + memberLineHeight / 2.0;
    double sceneY = mapToScene(QPointF(0, localY)).y();
    
    if (side == BoxSide::Left) {
        return QPointF(r.left(), sceneY);
    } else if (side == BoxSide::Right) {
        return QPointF(r.right(), sceneY);
    }
    return fallback;
}

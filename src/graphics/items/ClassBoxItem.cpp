#include "ClassBoxItem.h"
#include "RelationItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ClassBoxItem::ClassBoxItem(const RenderNode &node, const RenderTheme &theme)
    : m_node(node)
    , m_theme(theme)
    , m_initialPos(node.rect.topLeft())
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
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
    const double headerHeight = 35.0;
    
    bool isHovered = option->state & QStyle::State_MouseOver;
    bool isSelected = option->state & QStyle::State_Selected;
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    // 1. 如果选中，在卡片外部画一圈淡雅的靛蓝发光外环
    if (isSelected) {
        QPen selectPen(QColor(99, 102, 241, 120), 4.0);
        painter->setPen(selectPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(rect.adjusted(-2, -2, 2, 2), m_theme.nodeRadius + 1, m_theme.nodeRadius + 1);
    }
    
    // 2. 绘制类卡片大背景 (下半部为纯白，整体圆角)
    QPen borderPen(m_theme.primaryColor, m_theme.lineWidth);
    painter->setPen(borderPen);
    painter->setBrush(QBrush(m_theme.surfaceColor));
    painter->drawRoundedRect(rect, m_theme.nodeRadius, m_theme.nodeRadius);
    
    // 3. 绘制上半部的类名头部背景区 (圆角裁剪或局部绘制)
    // 悬浮时色调稍深
    QColor headerFill = isHovered ? QColor("#c7d2fe") : m_theme.primaryLightColor;
    painter->setBrush(QBrush(headerFill));
    
    // 使用 QPainterPath 来绘制只在上半部圆角、下半部平直的头部区域，保护下方卡片的圆角外观
    QPainterPath headerPath;
    QRectF headRect(rect.x(), rect.y(), rect.width(), headerHeight);
    headerPath.addRoundedRect(headRect, m_theme.nodeRadius, m_theme.nodeRadius);
    
    // 用一个平直的矩形把头部下半部的圆角遮盖切除，实现平滑分割
    QRectF clipRect(rect.x(), rect.y() + headerHeight - 8.0, rect.width(), 8.0);
    QPainterPath clipPath;
    clipPath.addRect(clipRect);
    headerPath = headerPath.united(clipPath);
    
    painter->drawPath(headerPath);
    
    // 4. 绘制类名区与成员区分割实线
    painter->setPen(QPen(m_theme.borderColor, m_theme.lineWidth));
    painter->drawLine(QPointF(rect.x(), rect.y() + headerHeight), QPointF(rect.x() + rect.width(), rect.y() + headerHeight));
    
    // 5. 绘制类名与元类修饰文本
    painter->setPen(m_theme.onSurfaceColor);
    QFont font = painter->font();
    font.setPointSize(10);
    font.setBold(true);
    painter->setFont(font);
    
    if (m_node.metaType == "interface" || m_node.metaType == "enum") {
        QFont metaFont = font;
        metaFont.setPointSize(8);
        metaFont.setBold(false);
        metaFont.setItalic(true);
        painter->setFont(metaFont);
        
        QString metaText = (m_node.metaType == "interface") ? "<<interface>>" : "<<enumeration>>";
        QRectF metaRect(rect.x(), rect.y() + 3, rect.width(), 14);
        painter->drawText(metaRect, Qt::AlignCenter, metaText);
        
        painter->setFont(font);
        QRectF nameRect(rect.x(), rect.y() + 15, rect.width(), 18);
        painter->drawText(nameRect, Qt::AlignCenter, m_node.id);
    } else {
        painter->drawText(headRect, Qt::AlignCenter, m_node.id);
    }
    
    // 6. 依次换行左对齐绘制类成员 (属性与方法)
    font.setPointSize(9);
    font.setBold(false);
    painter->setFont(font);
    painter->setPen(m_theme.onSurfaceMutedColor);
    
    double memberLineHeight = 18.0;
    for (int k = 0; k < m_node.members.size(); ++k) {
        double yOff = rect.y() + headerHeight + 6.0 + k * memberLineHeight;
        
        // 留出左右 10 像素的内边距，防贴边
        QRectF labelRect(rect.x() + 10.0, yOff, rect.width() - 20.0, memberLineHeight);
        painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, m_node.members[k]);
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

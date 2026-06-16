#include "ParticipantItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ParticipantItem::ParticipantItem(const RenderNode &node, const RenderTheme &theme)
    : m_node(node)
    , m_theme(theme)
{
    // 启用 hover 事件与选中状态支持
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QRectF ParticipantItem::boundingRect() const
{
    const QRectF &rect = m_node.rect;
    double h = rect.height() * 2.0 + m_node.lifelineLength;
    
    // 稍微外扩 6 像素的安全边界，防止线条笔触裁剪
    return QRectF(rect.x() - 6, rect.y() - 6, rect.width() + 12, h + 12);
}

void ParticipantItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF &topRect = m_node.rect;
    QRectF bottomRect(topRect.x(), topRect.y() + topRect.height() + m_node.lifelineLength, topRect.width(), topRect.height());
    
    // 1. 判断是否被 hover 或被选中
    bool isHovered = option->state & QStyle::State_MouseOver;
    bool isSelected = option->state & QStyle::State_Selected;
    
    // 悬浮时色彩渐深，未悬浮使用主题浅靛蓝
    QColor fillColor = isHovered ? QColor("#c7d2fe") : m_theme.primaryLightColor;
    
    // 2. 绘制生命线虚线
    painter->save();
    QPen dashPen(m_theme.primaryColor, m_theme.lineWidth, Qt::DashLine);
    painter->setPen(dashPen);
    QPointF startPt(topRect.x() + topRect.width() / 2.0, topRect.y() + topRect.height());
    QPointF endPt(bottomRect.x() + bottomRect.width() / 2.0, bottomRect.y());
    painter->drawLine(startPt, endPt);
    painter->restore();
    
    // 3. 绘制顶部/底部参与者框
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    // 如果被选中，先在顶底框周围绘制一层半透明的靛蓝光环
    if (isSelected) {
        QPen selectPen(QColor(99, 102, 241, 120), 4.0); // 粗光环
        painter->setPen(selectPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(topRect.adjusted(-2, -2, 2, 2), m_theme.nodeRadius + 1, m_theme.nodeRadius + 1);
        painter->drawRoundedRect(bottomRect.adjusted(-2, -2, 2, 2), m_theme.nodeRadius + 1, m_theme.nodeRadius + 1);
    }
    
    // 绘制主体框
    QPen borderPen(m_theme.primaryColor, m_theme.lineWidth);
    painter->setPen(borderPen);
    painter->setBrush(QBrush(fillColor));
    
    painter->drawRoundedRect(topRect, m_theme.nodeRadius, m_theme.nodeRadius);
    painter->drawRoundedRect(bottomRect, m_theme.nodeRadius, m_theme.nodeRadius);
    
    // 4. 绘制框体名字文本
    painter->setPen(m_theme.onSurfaceColor);
    
    // 设定精致的 10pt 加粗字体
    QFont font = painter->font();
    font.setPointSize(10);
    font.setBold(true);
    painter->setFont(font);
    
    painter->drawText(topRect, Qt::AlignCenter, m_node.displayName);
    painter->drawText(bottomRect, Qt::AlignCenter, m_node.displayName);
    
    painter->restore();
}

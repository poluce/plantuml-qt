#include "PackageGroupItem.h"
#include "ClassBoxItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

PackageGroupItem::PackageGroupItem(const RenderPackage &pkg, const RenderTheme &theme)
    : m_pkg(pkg)
    , m_theme(theme)
    , m_originalRect(pkg.rect)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
    // 强制将模块包背景图元放置在最下层，确保类图元和连线在上方绘制
    setZValue(-1.0);
    
    // 将物理绝对坐标转换到局部坐标绘制，设置 pos 为全局坐标，以支持拖动自适应
    setPos(pkg.rect.topLeft());
    m_pkg.rect = QRectF(0, 0, pkg.rect.width(), pkg.rect.height());
}

QRectF PackageGroupItem::boundingRect() const
{
    return m_pkg.rect.adjusted(-5, -5, 5, 5);
}

void PackageGroupItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    // 1. 获取包背景底色，默认为浅灰底色
    QColor bgColor("#f9fafb");
    if (!m_pkg.color.isEmpty()) {
        QColor specified(m_pkg.color);
        if (specified.isValid()) {
            bgColor = specified;
        }
    }
    
    // 2. 绘制精致的圆角矩形背景框
    QColor borderColor = bgColor.darker(110);
    painter->setPen(QPen(borderColor, 1.5, Qt::SolidLine));
    painter->setBrush(QBrush(bgColor));
    painter->drawRoundedRect(m_pkg.rect, 8.0, 8.0);
    
    // 3. 绘制包标题文字 (如 "Domain Layer (领域层)")
    painter->setPen(bgColor.darker(200));
    QFont font = painter->font();
    font.setPointSize(10);
    font.setBold(true);
    painter->setFont(font);
    
    QRectF titleRect = m_pkg.rect.adjusted(12.0, 10.0, -12.0, -m_pkg.rect.height() + 35.0);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_pkg.displayName);
    
    // 4. 绘制标题下方的灰色虚分割线
    painter->setPen(QPen(borderColor, 1.0, Qt::DashLine));
    double separatorY = m_pkg.rect.y() + 35.0;
    painter->drawLine(QPointF(m_pkg.rect.x() + 10.0, separatorY), QPointF(m_pkg.rect.x() + m_pkg.rect.width() - 10.0, separatorY));
    
    painter->restore();
}

QVariant PackageGroupItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        for (auto *child : childItems()) {
            if (auto *box = dynamic_cast<ClassBoxItem*>(child)) {
                box->updateEdges();
            }
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

void PackageGroupItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QPointF pos = event->pos();
    const QRectF &rect = m_pkg.rect;
    const double margin = 8.0;

    bool nearRight = (pos.x() >= rect.width() - margin && pos.x() <= rect.width() + margin);
    bool nearBottom = (pos.y() >= rect.height() - margin && pos.y() <= rect.height() + margin);

    if (nearRight && nearBottom) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (nearRight) {
        setCursor(Qt::SizeHorCursor);
    } else if (nearBottom) {
        setCursor(Qt::SizeVerCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    
    QGraphicsItem::hoverMoveEvent(event);
}

void PackageGroupItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent(event);
}

void PackageGroupItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->pos();
        const QRectF &rect = m_pkg.rect;
        const double margin = 8.0;

        bool nearRight = (pos.x() >= rect.width() - margin && pos.x() <= rect.width() + margin);
        bool nearBottom = (pos.y() >= rect.height() - margin && pos.y() <= rect.height() + margin);

        if (nearRight || nearBottom) {
            m_isResizing = true;
            m_pressPos = event->pos();
            m_startWidth = rect.width();
            m_startHeight = rect.height();
            
            if (nearRight && nearBottom) {
                m_resizeDir = RightBottom;
            } else if (nearRight) {
                m_resizeDir = Right;
            } else {
                m_resizeDir = Bottom;
            }
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

void PackageGroupItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isResizing) {
        QPointF delta = event->pos() - m_pressPos;
        double newWidth = m_startWidth;
        double newHeight = m_startHeight;
        
        double minWidth = 100.0;
        double minHeight = 80.0;
        
        QRectF childrenBounding;
        for (auto *child : childItems()) {
            QRectF childRectInParent = child->boundingRect().translated(child->pos());
            if (childrenBounding.isNull()) {
                childrenBounding = childRectInParent;
            } else {
                childrenBounding = childrenBounding.united(childRectInParent);
            }
        }
        
        if (!childrenBounding.isNull()) {
            minWidth = qMax(minWidth, childrenBounding.right() + 15.0);
            minHeight = qMax(minHeight, childrenBounding.bottom() + 15.0);
        }

        if (m_resizeDir == Right || m_resizeDir == RightBottom) {
            newWidth = qMax(minWidth, m_startWidth + delta.x());
        }
        if (m_resizeDir == Bottom || m_resizeDir == RightBottom) {
            newHeight = qMax(minHeight, m_startHeight + delta.y());
        }
        
        prepareGeometryChange();
        m_pkg.rect = QRectF(0, 0, newWidth, newHeight);
        update();
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void PackageGroupItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isResizing) {
        m_isResizing = false;
        m_resizeDir = None;
        event->accept();
        return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

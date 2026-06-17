#pragma once

#include <QGraphicsItem>
#include "../../render_model/RenderDocument.h"
#include "../../render_model/RenderTheme.h"

class PackageGroupItem : public QGraphicsItem
{
public:
    PackageGroupItem(const RenderPackage &pkg, const RenderTheme &theme);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QString id() const { return m_pkg.id; }

    // 获取未清零前的场景全局包范围，便于子图元定位
    QRectF originalRect() const { return m_originalRect; }

    // 获取当前在场景中的最新绝对包范围
    QRectF currentRect() const { return QRectF(scenePos(), m_pkg.rect.size()); }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    enum ResizeDirection {
        None,
        Right,
        Bottom,
        RightBottom
    };

    RenderPackage m_pkg;
    RenderTheme m_theme;
    QRectF m_originalRect;

    bool m_isResizing = false;
    ResizeDirection m_resizeDir = None;
    QPointF m_pressPos;
    double m_startWidth = 0.0;
    double m_startHeight = 0.0;
};

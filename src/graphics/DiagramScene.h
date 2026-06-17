#pragma once

#include <QGraphicsScene>
#include <QHash>
#include "../parser/SourceLocation.h"

class DiagramScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit DiagramScene(QObject *parent = nullptr);

    // 清空场景并重置索引缓存
    void clearDiagram();

    // 注册图元语义映射
    void registerItem(const QString &id, QGraphicsItem *item);

    // 根据语义ID寻找对应图元
    QGraphicsItem* itemBySemanticId(const QString &id) const;

signals:
    // 图元被激活（点击），发送语义ID和源码定位以供编辑器跳转
    void itemActivated(QString semanticId, SourceLocation location);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QHash<QString, QGraphicsItem*> m_itemById;
};

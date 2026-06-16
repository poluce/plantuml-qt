#include "DiagramScene.h"
#include "items/ParticipantItem.h"
#include "items/MessageArrowItem.h"

DiagramScene::DiagramScene(QObject *parent)
    : QGraphicsScene(parent)
{
    // 连接选择改变信号，打通右侧点击图元 -> 左侧编辑器同步跳转
    connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
        QList<QGraphicsItem*> items = selectedItems();
        if (items.isEmpty()) return;
        
        QGraphicsItem *item = items.first();
        
        // 1. 若点击的是参与者
        if (auto *pItem = dynamic_cast<ParticipantItem*>(item)) {
            emit itemActivated(pItem->id(), pItem->location());
        }
        // 2. 若点击的是消息箭头
        else if (auto *mItem = dynamic_cast<MessageArrowItem*>(item)) {
            emit itemActivated("", mItem->location());
        }
    });
}

void DiagramScene::clearDiagram()
{
    // 清空场景上的所有图元
    clear();
    // 清理语义 ID 索引缓存
    m_itemById.clear();
}

void DiagramScene::registerItem(const QString &id, QGraphicsItem *item)
{
    if (!id.isEmpty()) {
        m_itemById[id] = item;
    }
}

QGraphicsItem* DiagramScene::itemBySemanticId(const QString &id) const
{
    return m_itemById.value(id, nullptr);
}

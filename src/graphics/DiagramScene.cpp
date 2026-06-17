#include "DiagramScene.h"
#include "items/ParticipantItem.h"
#include "items/MessageArrowItem.h"
#include "items/ClassBoxItem.h"
#include "items/RelationItem.h"
#include "items/PackageGroupItem.h"
#include <QGraphicsSceneMouseEvent>
#include <QDebug>

DiagramScene::DiagramScene(QObject *parent)
    : QGraphicsScene(parent)
{
    // 连接选择改变信号，打通右侧点击图元 -> 左侧编辑器同步跳转
    connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
        QList<QGraphicsItem*> items = selectedItems();
        if (items.isEmpty()) return;
        
        QGraphicsItem *item = items.first();
        
        // 1. 时序图分支
        if (auto *pItem = dynamic_cast<ParticipantItem*>(item)) {
            qDebug() << "[DiagramScene] selectionChanged -> ParticipantItem"
                     << "ID:" << pItem->id()
                     << "sceneRect:" << pItem->sceneBoundingRect()
                     << "line:" << pItem->location().line;
            emit itemActivated(pItem->id(), pItem->location());
        }
        else if (auto *mItem = dynamic_cast<MessageArrowItem*>(item)) {
            qDebug() << "[DiagramScene] selectionChanged -> MessageArrowItem"
                     << "sceneRect:" << mItem->sceneBoundingRect()
                     << "line:" << mItem->location().line;
            emit itemActivated("", mItem->location());
        }
        // 2. 类图分支
        else if (auto *cItem = dynamic_cast<ClassBoxItem*>(item)) {
            qDebug() << "[DiagramScene] selectionChanged -> ClassBoxItem"
                     << "ID:" << cItem->id()
                     << "sceneRect:" << cItem->sceneRect()
                     << "line:" << cItem->location().line;
            emit itemActivated(cItem->id(), cItem->location());
        }
        else if (auto *rItem = dynamic_cast<RelationItem*>(item)) {
            qDebug() << "[DiagramScene] selectionChanged -> RelationItem"
                     << "from:" << rItem->fromNodeId()
                     << "to:" << rItem->toNodeId()
                     << "sceneRect:" << rItem->sceneRect()
                     << "line:" << rItem->location().line;
            emit itemActivated("", rItem->location());
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

void DiagramScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF scenePos = event->scenePos();
    QGraphicsItem *clickedItem = itemAt(scenePos, QTransform());
    if (clickedItem) {
        QString itemType = "UnknownItem";
        QString itemId = "";
        
        if (auto *cItem = dynamic_cast<ClassBoxItem*>(clickedItem)) {
            itemType = "ClassBoxItem";
            itemId = cItem->id();
            qDebug() << "[DiagramScene] 命中 ClassBoxItem"
                     << "ID:" << cItem->id()
                     << "sceneRect:" << cItem->sceneRect()
                     << "line:" << cItem->location().line;
        } else if (auto *rItem = dynamic_cast<RelationItem*>(clickedItem)) {
            itemType = "RelationItem";
            qDebug() << "[DiagramScene] 命中 RelationItem"
                     << "from:" << rItem->fromNodeId()
                     << "to:" << rItem->toNodeId()
                     << "sceneRect:" << rItem->sceneRect()
                     << "line:" << rItem->location().line;
        } else if (auto *pkgItem = dynamic_cast<PackageGroupItem*>(clickedItem)) {
            itemType = "PackageGroupItem";
            itemId = pkgItem->id();
            qDebug() << "[DiagramScene] 命中 PackageGroupItem"
                     << "ID:" << pkgItem->id()
                     << "sceneRect:" << pkgItem->sceneBoundingRect();
        } else if (auto *pItem = dynamic_cast<ParticipantItem*>(clickedItem)) {
            itemType = "ParticipantItem";
            itemId = pItem->id();
            qDebug() << "[DiagramScene] 命中 ParticipantItem"
                     << "ID:" << pItem->id()
                     << "sceneRect:" << pItem->sceneBoundingRect()
                     << "line:" << pItem->location().line;
        } else if (auto *mItem = dynamic_cast<MessageArrowItem*>(clickedItem)) {
            itemType = "MessageArrowItem";
            qDebug() << "[DiagramScene] 命中 MessageArrowItem"
                     << "sceneRect:" << mItem->sceneBoundingRect()
                     << "line:" << mItem->location().line;
        }
        
        if (!itemId.isEmpty()) {
            qDebug() << "[DiagramScene] 鼠标击中图元:" << itemType << "ID:" << itemId << "坐标:" << scenePos;
        } else {
            qDebug() << "[DiagramScene] 鼠标击中图元:" << itemType << "坐标:" << scenePos;
        }
    } else {
        qDebug() << "[DiagramScene] 鼠标点击于空白处，坐标:" << scenePos;
    }
    
    QGraphicsScene::mousePressEvent(event);
}

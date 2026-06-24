#include "DiagramSceneRenderer.h"
#include "items/ParticipantItem.h"
#include "items/MessageArrowItem.h"
#include "items/ClassBoxItem.h"
#include "items/RelationItem.h"
#include "items/NoteLineItem.h"
#include "items/PackageGroupItem.h"

void DiagramSceneRenderer::render(DiagramScene *scene, const RenderDocument &doc, const RenderTheme &theme)
{
    // 清空场景并重置缓存
    scene->clearDiagram();
    
    // 对称地向四周拓宽 2000 像素的画布边界，让用户可以极为宽裕地将任何边缘图元左键拖拽平移到视口中央
    const double margin = 2000.0;
    scene->setSceneRect(-margin, -margin, doc.width + margin * 2, doc.height + margin * 2);
    
    // 0. 依次将计算好几何的模块包底板图元渲染并加入场景 (置于背景层)
    QVector<PackageGroupItem*> packageItems;
    for (const auto &pkg : doc.packages) {
        auto *item = new PackageGroupItem(pkg, theme);
        scene->addItem(item);
        packageItems.append(item);
    }
    
    // 1. 依次将计算好几何的图元渲染为 ParticipantItem 或 ClassBoxItem
    for (const auto &node : doc.nodes) {
        QGraphicsItem *item = nullptr;
        if (node.kind == RenderNodeKind::ClassBox || node.kind == RenderNodeKind::Note) {
            item = new ClassBoxItem(node, theme);
        } else {
            item = new ParticipantItem(node, theme);
        }
        
        if (!item->scene()) {
            scene->addItem(item);
        }
        scene->registerItem(node.id, item);
    }
    
    // 2. 依次将计算好几何的消息线生成 MessageArrowItem、NoteLineItem 或 RelationItem
    for (const auto &edge : doc.edges) {
        QGraphicsItem *item = nullptr;
        if (edge.kind == RenderEdgeKind::NoteRelation) {
            auto *noteLine = new NoteLineItem(edge, theme);
            
            QGraphicsItem *fromNode = scene->itemBySemanticId(edge.fromNodeId);
            QGraphicsItem *toNode = scene->itemBySemanticId(edge.toNodeId);
            if (fromNode && toNode) {
                noteLine->setNodes(fromNode, toNode);
                if (auto *fromBox = dynamic_cast<ClassBoxItem*>(fromNode)) {
                    fromBox->addEdge(noteLine);
                }
                if (auto *toBox = dynamic_cast<ClassBoxItem*>(toNode)) {
                    toBox->addEdge(noteLine);
                }
            }
            item = noteLine;
        } else if (edge.kind == RenderEdgeKind::Inheritance || 
            edge.kind == RenderEdgeKind::Association ||
            edge.kind == RenderEdgeKind::Composition ||
            edge.kind == RenderEdgeKind::Aggregation ||
            edge.kind == RenderEdgeKind::Realization ||
            edge.kind == RenderEdgeKind::Dependency ||
            edge.kind == RenderEdgeKind::Nested) {
            auto *relItem = new RelationItem(edge, theme);
            
            QGraphicsItem *fromNode = scene->itemBySemanticId(edge.fromNodeId);
            QGraphicsItem *toNode = scene->itemBySemanticId(edge.toNodeId);
            if (fromNode && toNode) {
                relItem->setNodes(fromNode, toNode);
                if (auto *fromBox = dynamic_cast<ClassBoxItem*>(fromNode)) {
                    fromBox->addEdge(relItem);
                }
                if (auto *toBox = dynamic_cast<ClassBoxItem*>(toNode)) {
                    toBox->addEdge(relItem);
                }
            }
            item = relItem;
        } else {
            item = new MessageArrowItem(edge, theme);
        }
        
        scene->addItem(item);
    }
}

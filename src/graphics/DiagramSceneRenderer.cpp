#include "DiagramSceneRenderer.h"
#include "items/ParticipantItem.h"
#include "items/MessageArrowItem.h"

void DiagramSceneRenderer::render(DiagramScene *scene, const RenderDocument &doc, const RenderTheme &theme)
{
    // 清空场景并重置缓存
    scene->clearDiagram();
    
    // 设置场景大小，驱动 QGraphicsView 滚动条
    scene->setSceneRect(0, 0, doc.width, doc.height);
    
    // 1. 依次将计算好几何的参与者生成 ParticipantItem 图元
    for (const auto &node : doc.nodes) {
        auto *item = new ParticipantItem(node, theme);
        scene->addItem(item);
        
        // 注册到场景的语义映射表中
        scene->registerItem(node.id, item);
    }
    
    // 2. 依次将计算好几何的消息线生成 MessageArrowItem 图元
    for (const auto &edge : doc.edges) {
        auto *item = new MessageArrowItem(edge, theme);
        scene->addItem(item);
    }
}

#include "SequenceLayoutEngine.h"
#include <QHash>
#include <qmath.h>

RenderDocument SequenceLayoutEngine::layout(const SequenceDiagramAst &ast, const RenderTheme &theme)
{
    RenderDocument doc;
    
    // 布局度量常量 (按优雅浅色现代扁平风格比例)
    const double marginLeft = 80.0;
    const double marginTop = 50.0;
    const double participantSpacing = 180.0;
    const double headerWidth = 120.0;
    const double headerHeight = 40.0;
    const double lifelineTopOffset = marginTop + headerHeight;
    const double messageSpacing = 65.0;
    const double marginBottom = 80.0;
    
    int numParticipants = ast.participants.size();
    int numMessages = ast.statements.size();
    
    // 1. 根据消息数计算生命线的整体垂直向下延伸长度
    double lifelineLen = numMessages * messageSpacing + 40.0;
    if (numMessages == 0) {
        lifelineLen = 120.0; // 若无消息交互，生命线保留默认高度
    }
    
    // 用于记录各个 Participant ID 对应的中心 X 坐标，供消息线定位起止
    QHash<QString, double> participantX;
    
    // 2. 依次排开 Participant
    for (int i = 0; i < numParticipants; ++i) {
        const auto &p = ast.participants[i];
        
        double x = marginLeft + i * participantSpacing;
        double y = marginTop;
        
        RenderNode node;
        node.id = p.id;
        // 如果是双引号包起来的文字（例如 Lexer 扫出带空格的名字），直接赋予 DisplayName
        node.displayName = p.displayName;
        node.rect = QRectF(x, y, headerWidth, headerHeight);
        node.lifelineLength = lifelineLen;
        node.kind = RenderNodeKind::Participant;
        node.location = p.location;
        
        doc.nodes.append(node);
        
        // 记录其 Lifeline 中轴的 X 坐标
        participantX[p.id] = x + headerWidth / 2.0;
    }
    
    // 3. 依次布局水平交互消息线
    for (int i = 0; i < numMessages; ++i) {
        const auto &msg = ast.statements[i];
        
        double fromX = participantX.value(msg.from, marginLeft + headerWidth / 2.0);
        double toX = participantX.value(msg.to, marginLeft + headerWidth / 2.0);
        
        // 从生命线起点的下方 30px 开始垂直下坠排列消息
        double y = lifelineTopOffset + 30.0 + i * messageSpacing;
        
        RenderEdge edge;
        edge.fromNodeId = msg.from;
        edge.toNodeId = msg.to;
        edge.startPoint = QPointF(fromX, y);
        edge.endPoint = QPointF(toX, y);
        edge.label = msg.text;
        edge.kind = (msg.kind == MessageKind::Sync) ? RenderEdgeKind::SyncCall : RenderEdgeKind::ReplyCall;
        edge.location = msg.location;
        
        doc.edges.append(edge);
    }
    
    // 4. 计算最终画布宽高
    doc.width = marginLeft * 2.0 + qMax(0, numParticipants - 1) * participantSpacing + headerWidth;
    if (numParticipants == 0) {
        doc.width = 600.0;
    }
    
    // 总高度为：头部偏移 + 生命线长度 + 底部终结框高度 + 底部留白
    doc.height = lifelineTopOffset + lifelineLen + headerHeight + marginBottom;
    
    return doc;
}

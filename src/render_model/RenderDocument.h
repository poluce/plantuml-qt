#pragma once

#include <QString>
#include <QVector>
#include <QRectF>
#include <QPointF>
#include "../parser/SourceLocation.h"

enum class RenderNodeKind
{
    Participant
};

struct RenderNode
{
    QString id;
    QString displayName;
    QRectF rect;
    double lifelineLength = 0.0; // 生命线向下延伸长度
    RenderNodeKind kind = RenderNodeKind::Participant;
    SourceLocation location;
};

enum class RenderEdgeKind
{
    SyncCall,
    ReplyCall
};

struct RenderEdge
{
    QString fromNodeId;
    QString toNodeId;
    
    QPointF startPoint;
    QPointF endPoint;
    
    QString label;
    RenderEdgeKind kind = RenderEdgeKind::SyncCall;
    SourceLocation location;
};

struct RenderDocument
{
    QVector<RenderNode> nodes;
    QVector<RenderEdge> edges;
    
    double width = 0.0;
    double height = 0.0;
};

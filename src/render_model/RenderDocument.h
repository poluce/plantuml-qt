#pragma once

#include <QString>
#include <QVector>
#include <QRectF>
#include <QPointF>
#include "../parser/SourceLocation.h"

enum class RenderNodeKind
{
    Participant,
    ClassBox    // 新增: 类卡片盒子
};

struct RenderNode
{
    QString id;
    QString displayName;
    QRectF rect;
    double lifelineLength = 0.0; 
    QVector<QString> members; // 新增: 类图属性与方法成员
    QString metaType = "class"; // class, interface, enum
    RenderNodeKind kind = RenderNodeKind::Participant;
    SourceLocation location;
};

enum class RenderEdgeKind
{
    SyncCall,
    ReplyCall,
    Inheritance, // 类继承关系 (<|--)
    Composition, // 类组合关系 (*--)
    Aggregation, // 类聚合关系 (o--)
    Realization, // 类实现关系 (<|..)
    Dependency,  // 类依赖关系 (..>)
    Association  // 类关联关系 (-->)
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

struct RenderPackage
{
    QString id;
    QString displayName;
    QRectF rect;
    QString color;
};

struct RenderDocument
{
    QVector<RenderNode> nodes;
    QVector<RenderEdge> edges;
    QVector<RenderPackage> packages; // 新增: 模块包几何模型
    
    double width = 0.0;
    double height = 0.0;
};

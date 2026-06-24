#pragma once

#include <QString>
#include <QVector>
#include <QRectF>
#include <QPointF>
#include <QPainterPath>
#include "../parser/SourceLocation.h"
#include "../ast/DiagramAst.h"

enum class RenderNodeKind
{
    Participant,
    ClassBox,    // 新增: 类卡片盒子
    Note
};

struct RenderNode
{
    QString id;
    QString displayName;
    QRectF rect;
    double lifelineLength = 0.0; 
    QVector<ClassMember> members; // 类图属性与方法成员 (升级为 ClassMember)
    QString metaType = "class"; // class, interface, enum
    RenderNodeKind kind = RenderNodeKind::Participant;
    SourceLocation location;
    
    QString stereotype;
    QString style;
    
    // 关联类虚拟节点追踪两端实体节点ID
    QString assocFromNodeId;
    QString assocToNodeId;
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
    Association, // 类关联关系 (-->)
    NoteRelation, // 新增：Note 专属指向虚指示线
    Nested,      // 新增: 嵌套关系 (+--)
    AssociationLine, // 新增：无向普通关联线
    Square,      // 新增：正方形关系
    Cross,       // 新增：叉号关系
    Crowfoot,    // 新增：鸟爪关系
    Hat          // 新增：尖括号关系
};

struct RenderEdge
{
    QString fromNodeId;
    QString toNodeId;
    QString fromPort; // 新增：起始端成员端口/成员名
    QString toPort;   // 新增：终止端成员端口/成员名
    QString id;
    
    QPointF startPoint;
    QPointF endPoint;
    QVector<QPointF> points;
    QPainterPath path;
    
    QString label;
    QPointF labelPosition;
    bool hasLabelPosition = false;
    RenderEdgeKind kind = RenderEdgeKind::SyncCall;
    SourceLocation location;

    QString style;
    QString taillabel;
    QString headlabel;
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

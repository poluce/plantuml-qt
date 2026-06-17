#pragma once

#include <QString>
#include <QVector>
#include <memory>
#include "../parser/SourceLocation.h"

// ================= 时序图 AST 结构 =================
struct ParticipantDecl
{
    QString id;
    QString displayName;
    SourceLocation location;
};

enum class MessageKind
{
    Sync,   // -> 同步消息
    Reply   // --> 返回消息
};

struct MessageStatement
{
    QString from;
    QString to;
    QString text;
    MessageKind kind = MessageKind::Sync;
    SourceLocation location;
};

// ================= 类图 AST 结构 =================
struct ClassDecl
{
    QString id;
    QString metaType = "class"; // class, interface, enum
    QString packageName;        // 新增：归属包ID
    QVector<QString> members; // 类内的属性与方法列表
    SourceLocation location;
};

enum class RelationKind
{
    Inheritance, // 继承关系 <|--
    Composition, // 组合关系 *--
    Aggregation, // 聚合关系 o--
    Realization, // 实现关系 <|..
    Dependency,  // 依赖关系 ..>
    Association  // 关联关系 --> 或 ->
};

struct RelationDecl
{
    QString from; // 父类 / 源端
    QString to;   // 子类 / 宿端
    QString text; // 关系连线标签描述
    RelationKind kind = RelationKind::Association;
    SourceLocation location;
    QString direction; // 用于存储连线中指示的方向，以传递给布局计算
};

// ================= 统一 AST 抽象基类 =================
class DiagramAst
{
public:
    virtual ~DiagramAst() = default;
    
    // 运行时类型判定，方便 Graphviz 布局管线分流
    virtual bool isSequence() const = 0;
};

class SequenceDiagramAst : public DiagramAst
{
public:
    bool isSequence() const override { return true; }
    
    QVector<ParticipantDecl> participants;
    QVector<MessageStatement> statements;
};

class ClassDiagramAst : public DiagramAst
{
public:
    bool isSequence() const override { return false; }
    
    struct PackageDecl
    {
        QString id;
        QString displayName;
        QString color;
    };
    
    bool leftToRight = false; // 新增：记录是否是从左到右的全局排版流向
    
    QVector<ClassDecl> classes;
    QVector<RelationDecl> relations;
    QVector<PackageDecl> packages; // 新增：包容器列表
};

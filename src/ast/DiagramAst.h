#pragma once

#include <QString>
#include <QVector>
#include <QSet>
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
struct ClassMember
{
    QString rawText;       // 原始文本 (如 "+getName(): String")
    QString visibility;    // 可见性: "+", "-", "#", "~", ""
    bool isStatic = false; // 是否是静态成员 {static}
    bool isAbstract = false; // 是否是抽象成员 {abstract}
    bool isSeparator = false; // 是否是分割线 (如 "--", "..")
    QString separatorText;    // 分割线上的标题文本
    QString cleanText;        // 净化后的文本 (如 "getName(): String")
};

struct ClassDecl
{
    QString id;
    QString displayName;        // 显示的完整类名 (支持别名映射)
    QString metaType = "class"; // class, interface, enum, abstract class, annotation, entity 等
    QString packageName;        // 归属包ID
    QVector<ClassMember> members; // 升级为 ClassMember 列表
    SourceLocation location;
    QString stereotype;         // 原型修饰标签，如 <<Serializable>>
    QString style;              // 自定义样式
    QString generics;           // 泛型规范参数，例如 "T" 或 "K, V"
};

enum class RelationKind
{
    Inheritance, // 继承关系 <|--
    Composition, // 组合关系 *--
    Aggregation, // 聚合关系 o--
    Realization, // 实现关系 <|..
    Dependency,  // 依赖关系 ..>
    Association, // 关联关系 --> 或 ->
    Nested,      // 嵌套关系 +-
    AssociationLine, // 新增：无向普通关联线 (-- 或 ..)
    Square,      // 新增：正方形关系 #--
    Cross,       // 新增：叉号关系 x--
    Crowfoot,    // 新增：鸟爪/多端关系 }--
    Hat          // 新增：尖括号关系 ^--
};

struct RelationDecl
{
    QString from; // 父类 / 源端 ID/Alias
    QString to;   // 子类 / 宿端 ID/Alias
    QString fromMember; // 源端成员端点 (如 field1)
    QString toMember;   // 宿端成员端点 (如 field2)
    QString fromMultiplicity; // 源端多重度 (如 "1")
    QString toMultiplicity;   // 宿端多重度 (如 "*")
    QString fromQualifier;    // 源端端点限定词
    QString toQualifier;      // 宿端端点限定词
    QString text; // 关系连线标签描述
    RelationKind kind = RelationKind::Association;
    SourceLocation location;
    QString direction; // 用于存储连线中指示的方向，以传递给布局计算
    QString style;     // 存储中括号自定义样式，如 #red,dashed,thickness=4
};

struct AssociationClassDecl {
    QString classA;
    QString classB;
    QString assocClass;
    SourceLocation location;
};

struct NoteDecl
{
    QString id;        // 自定义 ID / 别名
    QString text;
    QString boundToId; // 绑定的类 ID。如果为空，则是独立 Note
    QString boundToMember; // 新增：绑定的成员名
    QString position;  // 方向: "left", "right", "top", "bottom"
    SourceLocation location;
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
    
    bool leftToRight = false; // 记录是否是从左到右的全局排版流向
    
    QSet<QString> hiddenElements;
    QSet<QString> removedElements;
    QVector<AssociationClassDecl> associationClasses;
    
    QVector<ClassDecl> classes;
    QVector<RelationDecl> relations;
    QVector<PackageDecl> packages; // 包容器列表
    QVector<NoteDecl> notes;       // 注释框列表
};

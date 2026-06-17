#pragma once

#include <QString>
#include "SourceLocation.h"

enum class TokenType
{
    StartUml,      // @startuml
    EndUml,        // @enduml
    Participant,   // participant
    Class,         // class 关键字
    LeftBrace,     // {
    RightBrace,    // }
    Identifier,    // 标识符 (User, A, B 等)
    Arrow,         // -> 同步消息/关联关系
    DottedArrow,   // --> 返回消息/指向关系
    Inherit,       // <|-- 继承关系
    Composition,   // *-- 组合关系
    Aggregation,   // o-- 聚合关系
    Realization,   // <|.. 实现关系
    Dependency,    // ..> 依赖关系
    InheritRight,  // --|> 右向继承
    RealizeRight,  // ..|> 右向实现
    Colon,         // :
    String,        // 连线/冒号后的内容
    Newline,       // 换行
    Eof,           // 源码结束
    Unknown        // 未知错误记号
};

struct Token
{
    TokenType type = TokenType::Unknown;
    QString value;
    SourceLocation location;
};

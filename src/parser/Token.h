#pragma once

#include <QString>
#include "SourceLocation.h"

enum class TokenType
{
    StartUml,      // @startuml
    EndUml,        // @enduml
    Participant,   // participant
    Identifier,    // 参与者标识符 (User, A, B 等)
    Arrow,         // -> 同步消息
    DottedArrow,   // --> 返回消息
    Colon,         // :
    String,        // 消息后面的文本内容
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

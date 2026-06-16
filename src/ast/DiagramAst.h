#pragma once

#include <QString>
#include <QVector>
#include <memory>
#include "../parser/SourceLocation.h"

struct ParticipantDecl
{
    QString id;
    QString displayName;
    SourceLocation location;
};

enum class MessageKind
{
    Sync,   // -> 同步调用
    Reply   // --> 异步/返回调用
};

struct MessageStatement
{
    QString from;
    QString to;
    QString text;
    MessageKind kind = MessageKind::Sync;
    SourceLocation location;
};

class DiagramAst
{
public:
    virtual ~DiagramAst() = default;
};

class SequenceDiagramAst : public DiagramAst
{
public:
    QVector<ParticipantDecl> participants;
    QVector<MessageStatement> statements;
};

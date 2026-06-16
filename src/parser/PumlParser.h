#pragma once

#include <QVector>
#include <memory>
#include "Token.h"
#include "ParseError.h"
#include "../ast/DiagramAst.h"

struct ParseResult
{
    std::unique_ptr<SequenceDiagramAst> ast;
    QVector<ParseError> errors;
};

class PumlParser
{
public:
    explicit PumlParser(const QVector<Token> &tokens);

    // 解析 Token 序列并输出 AST 与错误收集
    ParseResult parse();

private:
    Token peek() const;
    Token next();
    bool isEof() const;
    void skipNewlines();

    // 隐式补全参与者声明
    void ensureParticipantExists(SequenceDiagramAst *ast, const QString &id, const SourceLocation &loc);

    QVector<Token> m_tokens;
    int m_pos = 0;
};

#pragma once

#include <QVector>
#include <memory>
#include "Token.h"
#include "ParseError.h"
#include "../ast/DiagramAst.h"

struct ParseResult
{
    std::unique_ptr<DiagramAst> ast; // 返回多态基类指针
    QVector<ParseError> errors;
};

class PumlParser
{
public:
    explicit PumlParser(const QVector<Token> &tokens);

    // 解析 Token 序列并输出 AST 与错误收集 (运行时自动识别图类型分流)
    ParseResult parse();

private:
    Token peek() const;
    Token next();
    bool isEof() const;
    void skipNewlines();

    // 1. 时序图专有解析子程序
    std::unique_ptr<SequenceDiagramAst> parseSequenceDiagram(QVector<ParseError> &errors);
    void ensureParticipantExists(SequenceDiagramAst *ast, const QString &id, const SourceLocation &loc);

    // 2. 类图专有解析子程序
    std::unique_ptr<ClassDiagramAst> parseClassDiagram(QVector<ParseError> &errors);
    void ensureClassExists(ClassDiagramAst *ast, const QString &id, const SourceLocation &loc);

    QVector<Token> m_tokens;
    int m_pos = 0;
};

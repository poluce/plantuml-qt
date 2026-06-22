#pragma once

#include <QVector>
#include <memory>
#include <QString>
#include <QRegularExpression>
#include "ParseError.h"
#include "../ast/DiagramAst.h"

struct ParseResult
{
    std::unique_ptr<DiagramAst> ast; // 返回多态基类指针
    QVector<ParseError> errors;
};

struct ParserContext
{
    bool inClassBody = false;
    QString currentClassId;
    QVector<QString> packageStack; // 支持嵌套包的包名栈
};

// 抽象行命令基类
class LineCommand {
public:
    virtual ~LineCommand() = default;
    virtual bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) = 0;
};

class PumlParser
{
public:
    explicit PumlParser(const QString &sourceText);

    // 解析 PlantUML 并输出 AST 与错误收集 (支持类图与时序图)
    ParseResult parse();

private:
    void ensureClassExists(ClassDiagramAst *ast, const QString &id, const SourceLocation &loc);
    void ensureParticipantExists(SequenceDiagramAst *ast, const QString &id, const SourceLocation &loc);

    QString m_sourceText;
};

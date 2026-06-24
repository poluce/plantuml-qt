#pragma once

#include <QVector>
#include <QSet>
#include <memory>
#include <QString>
#include <QRegularExpression>
#include "ParseError.h"
#include "../ast/DiagramAst.h"

struct ParseResult
{
    std::unique_ptr<DiagramAst> ast; // 返回多态基类指针
    QVector<ParseError> errors;
    QStringList traceLog;
};

struct DiagramBlock {
    QString title;
    QString content;
    int startLine; // 在主文件中的 1-indexed 起始行号
};

QVector<DiagramBlock> splitDiagrams(const QString &sourceText);

struct ParserContext
{
    bool inClassBody = false;
    QString currentClassId;
    QVector<QString> packageStack; // 支持嵌套包的包名栈
    
    // 别名与真实类 ID 的映射缓存
    QHash<QString, QString> aliasToId;

    // 已声明类 ID 的集合
    QSet<QString> declaredClasses;

    // 多行 Note 解析状态机
    bool inNoteBody = false;
    QString currentNoteText;
    QString currentNoteBoundId;
    QString currentNoteBoundMember; // 新增：当前多行 Note 绑定的成员
    QString currentNotePos;
    QString currentNoteId;
    int noteStartLine = 0;
    int noteStartCol = 1;
    int noteStartLen = 0;

    // 多行 Legend 解析状态机
    bool inLegendBody = false;

    // 多行块注释状态机
    bool inCommentBlock = false;
};

// 抽象行命令基类
class LineCommand {
public:
    virtual ~LineCommand() = default;
    virtual bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors, int physicalCol, int physicalLen) = 0;
    virtual QString name() const = 0;
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

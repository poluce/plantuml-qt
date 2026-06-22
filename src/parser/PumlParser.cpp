#include "PumlParser.h"
#include <QDebug>
#include <QStack>

namespace {
    RelationKind parseRelationKind(const QString &sym)
    {
        if (sym == "<|--" || sym == "--|>") return RelationKind::Inheritance;
        if (sym == "<|.." || sym == "..|>") return RelationKind::Realization;
        if (sym == "*--") return RelationKind::Composition;
        if (sym == "o--") return RelationKind::Aggregation;
        if (sym == "..>") return RelationKind::Dependency;
        return RelationKind::Association;
    }

    QString normalizeDirection(const QString &dir) {
        if (dir.isEmpty()) return "";
        QString lower = dir.toLower();
        if (lower == "u" || lower == "up") return "up";
        if (lower == "d" || lower == "down") return "down";
        if (lower == "l" || lower == "left") return "left";
        if (lower == "r" || lower == "right") return "right";
        return lower;
    }

    QString flipDirection(const QString &dir) {
        if (dir.isEmpty()) return "";
        if (dir == "up") return "down";
        if (dir == "down") return "up";
        if (dir == "left") return "right";
        if (dir == "right") return "left";
        return dir;
    }
}

// ================= 各具体行解析命令实现 =================

// 1. 全局配置/注释/起止指令
class SkinParamCommand : public LineCommand {
public:
    SkinParamCommand() {
        m_regex = QRegularExpression("^(@startuml(?:\\s+.*)?|@enduml?|skinparam\\s+.*|left\\s+to\\s+right\\s+direction|set\\s+.*)$", QRegularExpression::CaseInsensitiveOption);
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(lineNum);
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;

        if (line.contains("left to right direction", Qt::CaseInsensitive)) {
            if (!ast->isSequence()) {
                static_cast<ClassDiagramAst*>(ast)->leftToRight = true;
            }
        }
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 2. 包模块声明命令
class PackageCommand : public LineCommand {
public:
    PackageCommand() {
        m_regex = QRegularExpression("^package\\s+(?:\"([^\"]+)\"|([^{\\s#]+))\\s*(#[0-9a-fA-F]{6}|#\\w+)?\\s*(\\{?)$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(lineNum);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;

        if (ast->isSequence()) return true; // 时序图直接跳过包处理

        QString pkgId = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        QString pkgColor = match.captured(3);
        bool hasBrace = (match.captured(4) == "{");

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);
        
        ClassDiagramAst::PackageDecl pkg;
        pkg.id = pkgId;
        pkg.displayName = pkgId;
        pkg.color = pkgColor;
        classAst->packages.append(pkg);

        if (hasBrace) {
            ctx.packageStack.push_back(pkgId);
        }
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 3. 右花括号闭合命令
class RightBraceCommand : public LineCommand {
public:
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(lineNum);
        Q_UNUSED(ast);
        Q_UNUSED(errors);
        if (line != "}") return false;

        if (ctx.inClassBody) {
            ctx.inClassBody = false;
            ctx.currentClassId.clear();
        } else if (!ctx.packageStack.isEmpty()) {
            ctx.packageStack.pop_back(); // 正确出栈，修复嵌套包归属 Bug
        }
        return true;
    }
};

// 4. 类声明命令
class ClassDeclCommand : public LineCommand {
public:
    ClassDeclCommand() {
        m_regex = QRegularExpression("^(class|interface|enum)\\s+(?:\"([^\"]+)\"|(\\w+))\\s*(?:<<.*>>)?\\s*(#[0-9a-fA-F]{6}|#\\w+)?\\s*(\\{?)$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;

        if (ast->isSequence()) {
            errors.append({SourceLocation{lineNum, 1, (int)line.length()}, "时序图中不支持类声明语句。"});
            return true;
        }

        QString meta = match.captured(1);
        QString classId = !match.captured(2).isEmpty() ? match.captured(2) : match.captured(3);
        bool hasBrace = (match.captured(5) == "{");

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);
        
        // 查重并更新或新建
        ClassDecl* targetDecl = nullptr;
        for (auto &c : classAst->classes) {
            if (c.id == classId) {
                targetDecl = &c;
                break;
            }
        }

        if (!targetDecl) {
            ClassDecl decl;
            decl.id = classId;
            decl.location = SourceLocation{lineNum, 1, (int)line.length()};
            classAst->classes.append(decl);
            targetDecl = &classAst->classes.last();
        }

        targetDecl->metaType = meta;
        targetDecl->location = SourceLocation{lineNum, 1, (int)line.length()};
        if (!ctx.packageStack.isEmpty()) {
            targetDecl->packageName = ctx.packageStack.last();
        }

        if (hasBrace) {
            ctx.inClassBody = true;
            ctx.currentClassId = classId;
        }
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 5-1. 标准类关系连线命令 (如 A --> B, A <|-- B, A -->[up] B)
class StandardRelationCommand : public LineCommand {
public:
    StandardRelationCommand() {
        m_regex = QRegularExpression("^(?:\"([^\"]+)\"|(\\w+))\\s*(<\\|--|\\*--|o--|<\\|\\.\\.|\\.\\.>|-->|->|--\\|>|\\.\\.\\|>)(?:\\[(up|down|left|right|u|d|l|r)\\])?\\s*(?:\"([^\"]+)\"|(\\w+))(?:\\s*:\\s*(.*))?$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        RelationDecl rel;
        rel.from = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        QString relSym = match.captured(3);
        rel.direction = normalizeDirection(match.captured(4));
        rel.to = !match.captured(5).isEmpty() ? match.captured(5) : match.captured(6);
        rel.text = match.captured(7);
        rel.kind = parseRelationKind(relSym);
        rel.location = SourceLocation{lineNum, 1, (int)line.length()};

        // 处理右向关系，进行起止点及方向对调归一化
        if (relSym == "--|>" || relSym == "..|>") {
            rel.from = rel.to;
            rel.to = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
            rel.direction = flipDirection(rel.direction);
        }

        classAst->relations.append(rel);
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 5-2. 夹带方向的实线关联连线命令 (如 A -up-> B, A --left-> B, A -down- B)
class HyphenRelationCommand : public LineCommand {
public:
    HyphenRelationCommand() {
        m_regex = QRegularExpression("^(?:\"([^\"]+)\"|(\\w+))\\s*(?:-+)(up|down|left|right|u|d|l|r)(?:-*)(->|-)\\s*(?:\"([^\"]+)\"|(\\w+))(?:\\s*:\\s*(.*))?$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        RelationDecl rel;
        rel.from = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        rel.direction = normalizeDirection(match.captured(3));
        rel.to = !match.captured(5).isEmpty() ? match.captured(5) : match.captured(6);
        rel.text = match.captured(7);
        rel.kind = RelationKind::Association;
        rel.location = SourceLocation{lineNum, 1, (int)line.length()};

        classAst->relations.append(rel);
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 5-3. 夹带方向的虚线依赖连线命令 (如 A .up.> B, A ..left. B)
class DotRelationCommand : public LineCommand {
public:
    DotRelationCommand() {
        m_regex = QRegularExpression("^(?:\"([^\"]+)\"|(\\w+))\\s*(?:\\.+)(up|down|left|right|u|d|l|r)(?:\\.*)(\\.>|\\.)\\s*(?:\"([^\"]+)\"|(\\w+))(?:\\s*:\\s*(.*))?$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        RelationDecl rel;
        rel.from = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        rel.direction = normalizeDirection(match.captured(3));
        rel.to = !match.captured(5).isEmpty() ? match.captured(5) : match.captured(6);
        rel.text = match.captured(7);
        rel.kind = RelationKind::Dependency;
        rel.location = SourceLocation{lineNum, 1, (int)line.length()};

        classAst->relations.append(rel);
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 5-4. 夹带方向的特殊几何连线命令 (继承/组合/聚合) (如 A <|--up- B, A -left-|>)
class SpecialRelationCommand : public LineCommand {
public:
    SpecialRelationCommand() {
        m_regex = QRegularExpression("^(?:\"([^\"]+)\"|(\\w+))\\s*(?:(<\\|\\*|o)-+(up|down|left|right|u|d|l|r)-+|-+(up|down|left|right|u|d|l|r)-*(\\|>))\\s*(?:\"([^\"]+)\"|(\\w+))(?:\\s*:\\s*(.*))?$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        RelationDecl rel;
        rel.from = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        
        QString symbolHead = match.captured(3);
        QString dir1 = match.captured(4);
        QString dir2 = match.captured(5);
        QString symbolTail = match.captured(6);
        
        rel.to = !match.captured(7).isEmpty() ? match.captured(7) : match.captured(8);
        rel.text = match.captured(9);
        rel.direction = normalizeDirection(!dir1.isEmpty() ? dir1 : dir2);
        rel.location = SourceLocation{lineNum, 1, (int)line.length()};

        if (symbolHead == "<|") {
            rel.kind = RelationKind::Inheritance;
        } else if (symbolHead == "*") {
            rel.kind = RelationKind::Composition;
        } else if (symbolHead == "o") {
            rel.kind = RelationKind::Aggregation;
        } else if (symbolTail == "|>") {
            rel.kind = RelationKind::Inheritance;
            // 右向关系，进行起止点对调归一化
            rel.from = rel.to;
            rel.to = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
            rel.direction = flipDirection(rel.direction);
        }

        classAst->relations.append(rel);
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 6. 时序图参与者声明命令
class ParticipantCommand : public LineCommand {
public:
    ParticipantCommand() {
        m_regex = QRegularExpression("^participant\\s+(?:\"([^\"]+)\"|(\\w+))(?:\\s+as\\s+(?:\"([^\"]+)\"|(\\w+)))?$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;

        if (!ast->isSequence()) {
            errors.append({SourceLocation{lineNum, 1, (int)line.length()}, "类图中不支持 participant 声明。"});
            return true;
        }

        QString firstVal = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        QString secondVal = !match.captured(3).isEmpty() ? match.captured(3) : match.captured(4);
        bool hasAs = !match.captured(3).isEmpty() || !match.captured(4).isEmpty();

        QString id;
        QString displayName;

        if (hasAs) {
            // 自适应提取 ID 和显示名称，完美支持两种别名格式
            // 格式 A: participant "Long Name" as Alias (第一个参数带引号，第二个不带)
            if (line.contains(QRegularExpression("participant\\s+\"[^\"]+\"\\s+as\\s+\\w+"))) {
                id = secondVal;
                displayName = firstVal;
            } else {
                // 格式 B: participant Alias as "Long Name" 或其它
                id = firstVal;
                displayName = secondVal;
            }
        } else {
            id = firstVal;
            displayName = firstVal;
        }

        SequenceDiagramAst* seqAst = static_cast<SequenceDiagramAst*>(ast);

        ParticipantDecl decl;
        decl.id = id;
        decl.displayName = displayName;
        decl.location = SourceLocation{lineNum, 1, (int)line.length()};
        seqAst->participants.append(decl);
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 7. 时序图消息连线命令
class MessageCommand : public LineCommand {
public:
    MessageCommand() {
        m_regex = QRegularExpression("^(?:\"([^\"]+)\"|(\\w+))\\s*(->|-->)\\s*(?:\"([^\"]+)\"|(\\w+))(?:\\s*:\\s*(.*))?$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;

        if (!ast->isSequence()) {
            errors.append({SourceLocation{lineNum, 1, (int)line.length()}, "类图中不支持时序图消息关系。"});
            return true;
        }

        QString fromVal = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        QString arrowSym = match.captured(3);
        QString toVal = !match.captured(4).isEmpty() ? match.captured(4) : match.captured(5);
        QString msgText = match.captured(6); // 冒号后文本可选，完美解决冒号必写 Bug

        SequenceDiagramAst* seqAst = static_cast<SequenceDiagramAst*>(ast);

        MessageStatement stmt;
        stmt.from = fromVal;
        stmt.to = toVal;
        stmt.text = msgText;
        stmt.kind = (arrowSym == "->") ? MessageKind::Sync : MessageKind::Reply;
        stmt.location = SourceLocation{lineNum, 1, (int)line.length()};
        
        seqAst->statements.append(stmt);
        return true;
    }
private:
    QRegularExpression m_regex;
};


// ================= PumlParser 实现 =================

PumlParser::PumlParser(const QString &sourceText)
    : m_sourceText(sourceText)
{
}

ParseResult PumlParser::parse()
{
    ParseResult result;

    // 1. 扫描所有非空行，判定图的类别 (Class 还是 Sequence)
    bool isClass = false;
    QStringList lines = m_sourceText.split('\n');
    for (const auto &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('\'')) continue;

        // 类图特征前瞻匹配
        if (trimmed.startsWith("class ") || trimmed.startsWith("interface ") ||
            trimmed.startsWith("enum ") || trimmed.startsWith("package ") ||
            trimmed.contains("left to right direction") ||
            trimmed.contains("<|--") || trimmed.contains("*--") ||
            trimmed.contains("o--") || trimmed.contains("<|..") ||
            trimmed.contains("..>") || trimmed.contains("--|>") ||
            trimmed.contains("..|>") ||
            trimmed.contains("-up->") || trimmed.contains("-down->") ||
            trimmed.contains("-left->") || trimmed.contains("-right->") ||
            trimmed.contains("-up-") || trimmed.contains("-down-") ||
            trimmed.contains("-left-") || trimmed.contains("-right-") ||
            trimmed.contains("-u->") || trimmed.contains("-d->") ||
            trimmed.contains("-l->") || trimmed.contains("-r->")) {
            isClass = true;
            break;
        }
    }

    if (isClass) {
        result.ast = std::make_unique<ClassDiagramAst>();
    } else {
        result.ast = std::make_unique<SequenceDiagramAst>();
    }

    // 2. 依次加载对应的行指令解析集 (栈上定义，解决 unique_ptr 拷贝限制)
    SkinParamCommand skinParam;
    PackageCommand package;
    ClassDeclCommand classDecl;
    
    // 4 个高内聚的连线指令子模块代替原单一指令类
    StandardRelationCommand stdRelation;
    HyphenRelationCommand hyphenRelation;
    DotRelationCommand dotRelation;
    SpecialRelationCommand specialRelation;

    RightBraceCommand rightBrace;
    ParticipantCommand participant;
    MessageCommand message;

    QVector<LineCommand*> commands = {
        &skinParam, &package, &classDecl, 
        &stdRelation, &hyphenRelation, &dotRelation, &specialRelation,
        &rightBrace, &participant, &message
    };

    ParserContext ctx;

    // 3. 遍历各行进行模式扫描
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty() || line.startsWith('\'')) continue;

        // 特殊处理：读取类成员大括号体内部
        if (ctx.inClassBody) {
            if (line == "}") {
                ctx.inClassBody = false;
                ctx.currentClassId.clear();
            } else {
                ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(result.ast.get());
                for (auto &c : classAst->classes) {
                    if (c.id == ctx.currentClassId) {
                        c.members.append(line);
                        break;
                    }
                }
            }
            continue;
        }

        // 常规指令匹配
        bool matched = false;
        for (auto *cmd : commands) {
            if (cmd->parse(line, i + 1, result.ast.get(), ctx, result.errors)) {
                matched = true;
                break;
            }
        }

        if (!matched) {
            result.errors.append({SourceLocation{i + 1, 1, (int)line.length()}, QString("无法识别的 PlantUML 语法: '%1'。").arg(line)});
        }
    }

    // 4. 时序图和类图的隐式实体关联补全
    if (isClass) {
        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(result.ast.get());
        for (const auto &rel : classAst->relations) {
            ensureClassExists(classAst, rel.from, rel.location);
            ensureClassExists(classAst, rel.to, rel.location);
        }
    } else {
        SequenceDiagramAst* seqAst = static_cast<SequenceDiagramAst*>(result.ast.get());
        for (const auto &stmt : seqAst->statements) {
            ensureParticipantExists(seqAst, stmt.from, stmt.location);
            ensureParticipantExists(seqAst, stmt.to, stmt.location);
        }
    }

    return result;
}

void PumlParser::ensureClassExists(ClassDiagramAst *ast, const QString &id, const SourceLocation &loc)
{
    for (const auto &c : ast->classes) {
        if (c.id == id) return;
    }
    ClassDecl decl;
    decl.id = id;
    decl.location = loc;
    ast->classes.append(decl);
}

void PumlParser::ensureParticipantExists(SequenceDiagramAst *ast, const QString &id, const SourceLocation &loc)
{
    for (const auto &p : ast->participants) {
        if (p.id == id) return;
    }
    ParticipantDecl decl;
    decl.id = id;
    decl.displayName = id;
    decl.location = loc;
    ast->participants.append(decl);
}

#include "PumlParser.h"
#include <QDebug>
#include <QStack>

namespace {
    RelationKind parseRelationKind(const QString &sym)
    {
        if (sym.contains("<|--") || sym.contains("--|>") || sym.contains("<|-") || sym.contains("-|>")) return RelationKind::Inheritance;
        if (sym.contains("<|..") || sym.contains("..|>") || sym.contains("<|.") || sym.contains(".|>")) return RelationKind::Realization;
        if (sym.contains("*--") || sym.contains("--*") || sym.contains("*-") || sym.contains("-*")) return RelationKind::Composition;
        if (sym.contains("o--") || sym.contains("--o") || sym.contains("o-") || sym.contains("-o")) return RelationKind::Aggregation;
        if (sym.contains("..>") || sym.contains("<..") || sym.contains(".>")) return RelationKind::Dependency;
        if (sym.contains("()") || sym.contains("()-") || sym.contains("-()")) return RelationKind::Association;
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

    ClassMember parseClassMember(const QString &line)
    {
        ClassMember m;
        m.rawText = line;

        QString trimmed = line.trimmed();

        // 匹配分割线，例如: -- title --，.. title ..，== title ==，__ title __
        // 或者仅仅是 --, .., ==, __
        QRegularExpression sepRegex("^(--+|\\.+|==+|__+)\\s*(.*?)\\s*\\1$");
        QRegularExpression sepRegexSimple("^(--+|\\.+|==+|__+)(.*)$");
        auto sepMatch = sepRegex.match(trimmed);
        if (sepMatch.hasMatch()) {
            m.isSeparator = true;
            m.separatorText = sepMatch.captured(2).trimmed();
            return m;
        }
        sepMatch = sepRegexSimple.match(trimmed);
        if (sepMatch.hasMatch()) {
            QString sym = sepMatch.captured(1);
            QString remain = sepMatch.captured(2).trimmed();
            if (remain.isEmpty() || sym.startsWith("-") || sym.startsWith(".") || sym.startsWith("=") || sym.startsWith("_")) {
                m.isSeparator = true;
                m.separatorText = remain;
                return m;
            }
        }

        // 提取可见性
        if (trimmed.startsWith('+')) {
            m.visibility = "+";
            trimmed = trimmed.mid(1).trimmed();
        } else if (trimmed.startsWith('-')) {
            m.visibility = "-";
            trimmed = trimmed.mid(1).trimmed();
        } else if (trimmed.startsWith('#')) {
            m.visibility = "#";
            trimmed = trimmed.mid(1).trimmed();
        } else if (trimmed.startsWith('~')) {
            m.visibility = "~";
            trimmed = trimmed.mid(1).trimmed();
        }

        // 提取 {static} 和 {abstract}
        if (trimmed.contains("{static}")) {
            m.isStatic = true;
            trimmed.replace("{static}", "");
        }
        if (trimmed.contains("{abstract}")) {
            m.isAbstract = true;
            trimmed.replace("{abstract}", "");
        }
        trimmed = trimmed.trimmed();

        return m;
    }

    void splitIdAndMember(const QString &raw, QString &id, QString &member)
    {
        QString clean = raw;
        if (clean.startsWith('"') && clean.endsWith('"')) {
            clean = clean.mid(1, clean.length() - 2);
        }
        int idx = clean.indexOf("::");
        if (idx != -1) {
            id = clean.left(idx).trimmed();
            member = clean.mid(idx + 2).trimmed();
        } else {
            id = clean.trimmed();
            member = "";
        }
    }

    QString extractRelationStyle(QString &cleanLine)
    {
        QString style;
        
        // 1. 提取连线内嵌的中括号样式，如 -[#red,dashed]->
        QRegularExpression inlineStyleRx("[-.]\\[([^\\]]*)\\][-.>]");
        auto match = inlineStyleRx.match(cleanLine);
        if (match.hasMatch()) {
            style = match.captured(1);
            QString fullMatch = match.captured(0);
            QString replacement = fullMatch;
            replacement.remove(QRegularExpression("\\[[^\\]]*\\]"));
            cleanLine.replace(fullMatch, replacement);
        }
        
        // 2. 提取连线行末尾的单行样式，如 A --> B #red;line.dashed
        QRegularExpression tailStyleRx("\\s+#((?:line:[^\\s:]+|line\\.[^\\s:]+|text:[^\\s:]+|[0-9a-fA-F]{6}|\\w+)(?:;[^\\s:]*)*)\\s*(?=:|$)");
        match = tailStyleRx.match(cleanLine);
        if (match.hasMatch()) {
            if (!style.isEmpty()) {
                style += ";";
            }
            style += match.captured(1);
            cleanLine.remove(match.captured(0));
        }
        
        return style;
    }
}

// ================= 各具体行解析命令实现 =================

// 1. 全局配置/注释/起止指令
// 1. 全局配置/注释/起止指令
class SkinParamCommand : public LineCommand {
public:
    QString name() const override { return "SkinParamCommand"; }
    SkinParamCommand() {
        m_regex = QRegularExpression("^(@startuml(?:\\s+.*)?|@enduml?|skinparam\\s+.*|left\\s+to\\s+right\\s+direction.*|top\\s+to\\s+bottom\\s+direction.*|set\\s+.*|page\\s+.*|stereotype\\s+.*)$", QRegularExpression::CaseInsensitiveOption);
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
        } else if (line.contains("top to bottom direction", Qt::CaseInsensitive)) {
            if (!ast->isSequence()) {
                static_cast<ClassDiagramAst*>(ast)->leftToRight = false;
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
    QString name() const override { return "PackageCommand"; }
    PackageCommand() {
        m_regex = QRegularExpression("^(?:package|namespace)\\s+(?:\"([^\"]+)\"|([^{\\s#]+))\\s*(?:<<[^>]+>>)?\\s*(#[^\\s{}#]+)?\\s*(\\{?)$");
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
    QString name() const override { return "RightBraceCommand"; }
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
    QString name() const override { return "ClassDeclCommand"; }
    ClassDeclCommand() {
        m_regex = QRegularExpression("^(abstract\\s+class|class|interface|enum|annotation|entity|abstract|struct|protocol|exception|metaclass|circle|\\(\\)|diamond|<>|json|object)\\s+(?:\"([^\"]+)\"|([^\"\\s{}]+))(?:\\s+as\\s+([^\"\\s{}]+))?\\s*(?:<<([^>]+)>>)?\\s*(#(?:line:[^;\\s{}#]+|back:[^;\\s{}#]+|header:[^;\\s{}#]+|[0-9a-fA-F]{6}|[^;\\s{}#]+))?\\s*(?:##\\[([^\\]]*)\\](\\w+|#[0-9a-fA-F]{6}|#\\w+))?\\s*([^;\\{\\n]*)?\\s*(?:;([^\\{\\n]*))?\\s*(\\{?)$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;

        if (ast->isSequence()) {
            errors.append({SourceLocation{lineNum, 1, (int)line.length()}, "时序图中不支持类声明语句。"});
            return true;
        }

        QString meta = match.captured(1);
        QString rawClassId = !match.captured(2).isEmpty() ? match.captured(2) : match.captured(3);
        QString alias = match.captured(4);
        QString stereotype = match.captured(5);
        QString baseColor = match.captured(6);
        QString borderStyle = match.captured(7);
        QString borderColor = match.captured(8);
        QString extraStyle = match.captured(10);
        bool hasBrace = (match.captured(11) == "{");

        QString classId = rawClassId;
        QString displayName = rawClassId;
        if (!alias.isEmpty()) {
            classId = alias;
            ctx.aliasToId[alias] = rawClassId;
        }

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
        targetDecl->displayName = displayName;
        targetDecl->location = SourceLocation{lineNum, 1, (int)line.length()};
        targetDecl->stereotype = stereotype;

        QStringList styles;
        if (!baseColor.isEmpty()) {
            styles << QString("back:%1").arg(baseColor);
        }
        if (!borderStyle.isEmpty()) {
            styles << QString("border.style:%1").arg(borderStyle);
        }
        if (!borderColor.isEmpty()) {
            styles << QString("border.color:%1").arg(borderColor);
        }
        if (!extraStyle.isEmpty()) {
            styles << extraStyle;
        }
        targetDecl->style = styles.join(";");

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
    QString name() const override { return "StandardRelationCommand"; }
    StandardRelationCommand() {
        m_regex = QRegularExpression(
            "^(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*"  // 1, 2: from
            "(?:\\[([^\\]]*)\\])?\\s*"            // 3: fromQualifier
            "(?:\"([^\"]*)\"\\s*)?"               // 4: fromMultiplicity
            "([<*o#x^+|()<>{}._-]*[-.]+[<*o#x^+|()<>{}._-]*)" // 5: relSym
            "(?:\\[(up|down|left|right|u|d|l|r)\\])?\\s*" // 6: direction
            "(?:\"([^\"]*)\"\\s*)?"               // 7: toMultiplicity
            "(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*"  // 8, 9: to
            "(?:\\[([^\\]]*)\\])?\\s*"            // 10: toQualifier
            "(?:\\s*:\\s*(.*))?$"                 // 11: text
        );
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        
        QString cleanLine = line;
        QString style = extractRelationStyle(cleanLine);

        auto match = m_regex.match(cleanLine);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        QString fromRaw = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        QString fromQualifier = match.captured(3);
        QString fromMultiplicity = match.captured(4);
        QString relSym = match.captured(5);
        QString direction = normalizeDirection(match.captured(6));
        QString toMultiplicity = match.captured(7);
        QString toRaw = !match.captured(8).isEmpty() ? match.captured(8) : match.captured(9);
        QString toQualifier = match.captured(10);
        QString text = match.captured(11);

        RelationDecl rel;
        splitIdAndMember(fromRaw, rel.from, rel.fromMember);
        splitIdAndMember(toRaw, rel.to, rel.toMember);
        rel.fromQualifier = fromQualifier;
        rel.toQualifier = toQualifier;
        rel.fromMultiplicity = fromMultiplicity;
        rel.toMultiplicity = toMultiplicity;
        rel.direction = direction;
        rel.text = text;
        rel.kind = parseRelationKind(relSym);
        rel.location = SourceLocation{lineNum, 1, (int)line.length()};
        rel.style = style;

        // 处理右向关系，进行起止点及方向对调归一化
        if (!relSym.startsWith("<") && (relSym.endsWith("|>") || relSym.endsWith(">"))) {
            std::swap(rel.from, rel.to);
            std::swap(rel.fromMember, rel.toMember);
            std::swap(rel.fromQualifier, rel.toQualifier);
            std::swap(rel.fromMultiplicity, rel.toMultiplicity);
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
    QString name() const override { return "HyphenRelationCommand"; }
    HyphenRelationCommand() {
        m_regex = QRegularExpression(
            "^(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*"  // 1, 2: from
            "(?:\\[([^\\]]*)\\])?\\s*"            // 3: fromQualifier
            "(?:\"([^\"]*)\"\\s*)?"               // 4: fromMultiplicity
            "(-*)(up|down|left|right|u|d|l|r)(-*)(->|-)" // 5, 6, 7, 8: direction / relSym
            "\\s*(?:\"([^\"]*)\"\\s*)?"           // 9: toMultiplicity
            "(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*"  // 10, 11: to
            "(?:\\[([^\\]]*)\\])?\\s*"            // 12: toQualifier
            "(?:\\s*:\\s*(.*))?$"                 // 13: text
        );
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);

        QString cleanLine = line;
        QString style = extractRelationStyle(cleanLine);

        auto match = m_regex.match(cleanLine);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        QString fromRaw = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        QString fromQualifier = match.captured(3);
        QString fromMultiplicity = match.captured(4);
        QString direction = normalizeDirection(match.captured(6));
        QString toMultiplicity = match.captured(9);
        QString toRaw = !match.captured(10).isEmpty() ? match.captured(10) : match.captured(11);
        QString toQualifier = match.captured(12);
        QString text = match.captured(13);

        RelationDecl rel;
        splitIdAndMember(fromRaw, rel.from, rel.fromMember);
        splitIdAndMember(toRaw, rel.to, rel.toMember);
        rel.fromQualifier = fromQualifier;
        rel.toQualifier = toQualifier;
        rel.fromMultiplicity = fromMultiplicity;
        rel.toMultiplicity = toMultiplicity;
        rel.direction = direction;
        rel.text = text;
        rel.kind = RelationKind::Association;
        rel.location = SourceLocation{lineNum, 1, (int)line.length()};
        rel.style = style;

        classAst->relations.append(rel);
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 5-3. 夹带方向的虚线依赖连线命令 (如 A .up.> B, A ..left. B)
class DotRelationCommand : public LineCommand {
public:
    QString name() const override { return "DotRelationCommand"; }
    DotRelationCommand() {
        m_regex = QRegularExpression(
            "^(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*"  // 1, 2: from
            "(?:\\[([^\\]]*)\\])?\\s*"            // 3: fromQualifier
            "(?:\"([^\"]*)\"\\s*)?"               // 4: fromMultiplicity
            "(\\.*)(up|down|left|right|u|d|l|r)(\\.*)(\\.>|\\.)" // 5, 6, 7, 8: direction / relSym
            "\\s*(?:\"([^\"]*)\"\\s*)?"           // 9: toMultiplicity
            "(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*"  // 10, 11: to
            "(?:\\[([^\\]]*)\\])?\\s*"            // 12: toQualifier
            "(?:\\s*:\\s*(.*))?$"                 // 13: text
        );
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);

        QString cleanLine = line;
        QString style = extractRelationStyle(cleanLine);

        auto match = m_regex.match(cleanLine);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        QString fromRaw = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        QString fromQualifier = match.captured(3);
        QString fromMultiplicity = match.captured(4);
        QString direction = normalizeDirection(match.captured(6));
        QString toMultiplicity = match.captured(9);
        QString toRaw = !match.captured(10).isEmpty() ? match.captured(10) : match.captured(11);
        QString toQualifier = match.captured(12);
        QString text = match.captured(13);

        RelationDecl rel;
        splitIdAndMember(fromRaw, rel.from, rel.fromMember);
        splitIdAndMember(toRaw, rel.to, rel.toMember);
        rel.fromQualifier = fromQualifier;
        rel.toQualifier = toQualifier;
        rel.fromMultiplicity = fromMultiplicity;
        rel.toMultiplicity = toMultiplicity;
        rel.direction = direction;
        rel.text = text;
        rel.kind = RelationKind::Dependency;
        rel.location = SourceLocation{lineNum, 1, (int)line.length()};
        rel.style = style;

        classAst->relations.append(rel);
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 5-4. 夹带方向的特殊几何连线命令 (继承/组合/聚合) (如 A <|--up- B, A -left-|>)
class SpecialRelationCommand : public LineCommand {
public:
    QString name() const override { return "SpecialRelationCommand"; }
    SpecialRelationCommand() {
        m_regex = QRegularExpression(
            "^(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*"  // 1, 2: from
            "(?:\\[([^\\]]*)\\])?\\s*"            // 3: fromQualifier
            "(?:\"([^\"]*)\"\\s*)?"               // 4: fromMultiplicity
            "(?:((?:<\\||\\*|o)-*)(up|down|left|right|u|d|l|r)(-*)|(-*)(up|down|left|right|u|d|l|r)(-*\\|>))" // 5, 6, 7, 8, 9, 10, 11
            "\\s*(?:\"([^\"]*)\"\\s*)?"           // 12: toMultiplicity
            "(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*"  // 13, 14: to
            "(?:\\[([^\\]]*)\\])?\\s*"            // 15: toQualifier
            "(?:\\s*:\\s*(.*))?$"                 // 16: text
        );
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);

        QString cleanLine = line;
        QString style = extractRelationStyle(cleanLine);

        auto match = m_regex.match(cleanLine);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        QString fromRaw = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        QString fromQualifier = match.captured(3);
        QString fromMultiplicity = match.captured(4);
        
        QString symbolHead = match.captured(6);
        QString dir1 = match.captured(7);
        QString dir2 = match.captured(10);
        QString symbolTail = match.captured(11);
        
        QString toMultiplicity = match.captured(12);
        QString toRaw = !match.captured(13).isEmpty() ? match.captured(13) : match.captured(14);
        QString toQualifier = match.captured(15);
        QString text = match.captured(16);

        RelationDecl rel;
        splitIdAndMember(fromRaw, rel.from, rel.fromMember);
        splitIdAndMember(toRaw, rel.to, rel.toMember);
        rel.fromQualifier = fromQualifier;
        rel.toQualifier = toQualifier;
        rel.fromMultiplicity = fromMultiplicity;
        rel.toMultiplicity = toMultiplicity;
        rel.direction = normalizeDirection(!dir1.isEmpty() ? dir1 : dir2);
        rel.text = text;
        rel.location = SourceLocation{lineNum, 1, (int)line.length()};
        rel.style = style;

        if (symbolHead.startsWith("<|")) {
            rel.kind = RelationKind::Inheritance;
        } else if (symbolHead.startsWith("*")) {
            rel.kind = RelationKind::Composition;
        } else if (symbolHead.startsWith("o")) {
            rel.kind = RelationKind::Aggregation;
        } else if (symbolTail.endsWith("|>")) {
            rel.kind = RelationKind::Inheritance;
            // 右向关系，对调归一化
            std::swap(rel.from, rel.to);
            std::swap(rel.fromMember, rel.toMember);
            std::swap(rel.fromQualifier, rel.toQualifier);
            std::swap(rel.fromMultiplicity, rel.toMultiplicity);
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
    QString name() const override { return "ParticipantCommand"; }
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
    QString name() const override { return "MessageCommand"; }
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
        QString msgText = match.captured(6);

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

// 8. 预处理指令命令
class PreprocessCommand : public LineCommand {
public:
    QString name() const override { return "PreprocessCommand"; }
    PreprocessCommand() {
        m_regex = QRegularExpression("^!.*$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(lineNum);
        Q_UNUSED(ast);
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        return m_regex.match(line).hasMatch();
    }
private:
    QRegularExpression m_regex;
};

// 9. 标题命令
class TitleCommand : public LineCommand {
public:
    QString name() const override { return "TitleCommand"; }
    TitleCommand() {
        m_regex = QRegularExpression("^title(?:\\s+(.*))?$", QRegularExpression::CaseInsensitiveOption);
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(lineNum);
        Q_UNUSED(ast);
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        return m_regex.match(line).hasMatch();
    }
private:
    QRegularExpression m_regex;
};

// 10. 图例开始命令
class LegendCommand : public LineCommand {
public:
    QString name() const override { return "LegendCommand"; }
    LegendCommand() {
        m_regex = QRegularExpression("^legend(?:\\s+(left|right|center))?$", QRegularExpression::CaseInsensitiveOption);
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(lineNum);
        Q_UNUSED(ast);
        Q_UNUSED(errors);
        if (m_regex.match(line).hasMatch()) {
            ctx.inLegendBody = true;
            return true;
        }
        return false;
    }
private:
    QRegularExpression m_regex;
};

// 11. 图表隐藏/删除控制命令
class ControlCommand : public LineCommand {
public:
    QString name() const override { return "ControlCommand"; }
    ControlCommand() {
        m_regex = QRegularExpression("^(hide|show|remove|restore)\\s+(.*)$", QRegularExpression::CaseInsensitiveOption);
        m_targetRegex = QRegularExpression("^(?:\"([^\"]+)\"|([^\\s{}#]+))$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(lineNum);
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;

        if (ast->isSequence()) return true;

        QString action = match.captured(1).toLower();
        QString targetPart = match.captured(2).trimmed();

        auto targetMatch = m_targetRegex.match(targetPart);
        if (targetMatch.hasMatch()) {
            QString target = !targetMatch.captured(1).isEmpty() ? targetMatch.captured(1) : targetMatch.captured(2);
            ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);
            if (action == "hide") {
                classAst->hiddenElements.insert(target);
            } else if (action == "remove") {
                classAst->removedElements.insert(target);
            }
        }
        return true; // 即使是全局控制，也静默返回 true
    }
private:
    QRegularExpression m_regex;
    QRegularExpression m_targetRegex;
};

// 12. 关系类 (Association Class) 声明命令
class AssociationClassCommand : public LineCommand {
public:
    QString name() const override { return "AssociationClassCommand"; }
    AssociationClassCommand() {
        m_regex = QRegularExpression("^\\(\\s*(?:\"([^\"]+)\"|([^\\s,()]+))\\s*,\\s*(?:\"([^\"]+)\"|([^\\s,()]+))\\s*\\)\\s*(\\.\\.+|-+)\\s*(?:\"([^\"]+)\"|([^\\s,()]+))\\s*$", QRegularExpression::CaseInsensitiveOption);
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        AssociationClassDecl assoc;
        assoc.classA = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        assoc.classB = !match.captured(3).isEmpty() ? match.captured(3) : match.captured(4);
        assoc.assocClass = !match.captured(6).isEmpty() ? match.captured(6) : match.captured(7);
        assoc.location = SourceLocation{lineNum, 1, (int)line.length()};

        classAst->associationClasses.append(assoc);
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 13. 单行继承与实现声明命令
class ExtendsImplementsCommand : public LineCommand {
public:
    QString name() const override { return "ExtendsImplementsCommand"; }
    ExtendsImplementsCommand() {
        m_regex = QRegularExpression("^(class|interface)\\s+(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s+(extends|implements)\\s+(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*$", QRegularExpression::CaseInsensitiveOption);
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        QString childId = !match.captured(2).isEmpty() ? match.captured(2) : match.captured(3);
        QString relType = match.captured(4).toLower();
        QString parentId = !match.captured(5).isEmpty() ? match.captured(5) : match.captured(6);

        RelationDecl rel;
        rel.from = parentId;
        rel.to = childId;
        rel.kind = (relType == "extends") ? RelationKind::Inheritance : RelationKind::Realization;
        rel.location = SourceLocation{lineNum, 1, (int)line.length()};

        classAst->relations.append(rel);
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 13.5. together 排版聚集命令
class TogetherCommand : public LineCommand {
public:
    QString name() const override { return "TogetherCommand"; }
    TogetherCommand() {
        m_regex = QRegularExpression("^together\\s*\\{?$", QRegularExpression::CaseInsensitiveOption);
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(lineNum);
        Q_UNUSED(ast);
        Q_UNUSED(errors);
        if (m_regex.match(line).hasMatch()) {
            if (line.contains("{")) {
                ctx.packageStack.push_back("__together__");
            }
            return true;
        }
        return false;
    }
private:
    QRegularExpression m_regex;
};

// 13.6. 类样式自定义声明命令 (如 bar类1 : [bold])
class ClassStyleCommand : public LineCommand {
public:
    QString name() const override { return "ClassStyleCommand"; }
    ClassStyleCommand() {
        m_regex = QRegularExpression("^(?:\"([^\"]+)\"|([^\"\\s{}]+))\\s*:\\s*\\[([^\\]]+)\\]$");
    }
    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(lineNum);
        Q_UNUSED(ctx);
        Q_UNUSED(errors);
        auto match = m_regex.match(line);
        if (!match.hasMatch()) return false;
        if (ast->isSequence()) return false;

        QString classId = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        QString style = match.captured(3);

        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);
        for (auto &c : classAst->classes) {
            if (c.id == classId) {
                if (!c.style.isEmpty()) {
                    c.style += ";";
                }
                c.style += style;
                break;
            }
        }
        return true;
    }
private:
    QRegularExpression m_regex;
};

// 14. 悬空 Note 与连线 Note 声明命令
class NoteCommand : public LineCommand {
public:
    QString name() const override { return "NoteCommand"; }
    NoteCommand() {
        m_rxSingleFloating = QRegularExpression("^note\\s+\"([^\"]+)\"\\s+as\\s+(\\w+)(?:\\s*(#\\S+))?$");
        m_rxMultiFloatingStart = QRegularExpression("^note\\s+as\\s+(\\w+)(?:\\s*(#\\S+))?$");
        m_rxSingleLink = QRegularExpression("^note\\s+(?:(left|right|top|bottom)\\s+)?on\\s+link(?:\\s*(#\\S+))?\\s*:\\s*(.*)$");
        m_rxMultiLinkStart = QRegularExpression("^note\\s+(?:(left|right|top|bottom)\\s+)?on\\s+link(?:\\s*(#\\S+))?\\s*$");
        
        // 绑定 Note 正则：注意 C++ 里只需要双重转义 \\s
        m_rxSingleBound = QRegularExpression("^note\\s+(left|right|top|bottom)\\s+of\\s+(?:\"([^\"]+)\"|([^\\s#:]+(?:::(?:\"[^\"]+\"|[^\\s#:]+))?))\\s*(?:(#\\S+))?\\s*(?<!:):(?!:)\\s*(.*)$");
        m_rxMultiBoundStart = QRegularExpression("^note\\s+(left|right|top|bottom)\\s+of\\s+(?:\"([^\"]+)\"|([^\\s#:]+(?:::(?:\"[^\"]+\"|[^\\s#:]+))?))\\s*(?:(#\\S+))?\\s*$");
        
        // 附加到上一个类的 Note
        m_rxSingleAttached = QRegularExpression("^note\\s+(left|right|top|bottom)(?:\\s*(#\\S+))?\\s*:\\s*(.*)$");
        m_rxMultiAttachedStart = QRegularExpression("^note\\s+(left|right|top|bottom)(?:\\s*(#\\S+))?\\s*$");
    }

    bool parse(const QString &line, int lineNum, DiagramAst *ast, ParserContext &ctx, QVector<ParseError> &errors) override {
        Q_UNUSED(errors);
        ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(ast);

        // 1. 单行悬空 Note
        auto match = m_rxSingleFloating.match(line);
        if (match.hasMatch()) {
            NoteDecl note;
            note.id = match.captured(2);
            note.text = match.captured(1);
            note.location = SourceLocation{lineNum, 1, (int)line.length()};
            classAst->notes.append(note);
            return true;
        }

        // 2. 多行悬空 Note 开始
        match = m_rxMultiFloatingStart.match(line);
        if (match.hasMatch()) {
            ctx.inNoteBody = true;
            ctx.currentNoteId = match.captured(1);
            ctx.currentNoteBoundId.clear();
            ctx.currentNoteBoundMember.clear();
            ctx.currentNotePos.clear();
            ctx.currentNoteText.clear();
            ctx.noteStartLine = lineNum;
            return true;
        }

        // 3. 单行连线 Note
        match = m_rxSingleLink.match(line);
        if (match.hasMatch()) {
            NoteDecl note;
            note.position = normalizeDirection(match.captured(1));
            note.text = match.captured(3);
            note.location = SourceLocation{lineNum, 1, (int)line.length()};
            if (!classAst->relations.isEmpty()) {
                note.boundToId = classAst->relations.last().from;
            }
            classAst->notes.append(note);
            return true;
        }

        // 4. 多行连线 Note 开始
        match = m_rxMultiLinkStart.match(line);
        if (match.hasMatch()) {
            ctx.inNoteBody = true;
            ctx.currentNoteId.clear();
            ctx.currentNoteBoundId.clear();
            ctx.currentNoteBoundMember.clear();
            if (!classAst->relations.isEmpty()) {
                ctx.currentNoteBoundId = classAst->relations.last().from;
            }
            ctx.currentNotePos = normalizeDirection(match.captured(1));
            ctx.currentNoteText.clear();
            ctx.noteStartLine = lineNum;
            return true;
        }

        // 5. 单行绑定 Note
        match = m_rxSingleBound.match(line);
        if (match.hasMatch()) {
            NoteDecl note;
            note.position = normalizeDirection(match.captured(1));
            QString rawTarget = !match.captured(2).isEmpty() ? match.captured(2) : match.captured(3);
            QString targetId, targetMember;
            splitIdAndMember(rawTarget, targetId, targetMember);
            note.boundToId = targetId;
            note.boundToMember = targetMember;
            note.text = match.captured(5);
            note.location = SourceLocation{lineNum, 1, (int)line.length()};
            classAst->notes.append(note);
            return true;
        }

        // 6. 多行绑定 Note 开始
        match = m_rxMultiBoundStart.match(line);
        if (match.hasMatch()) {
            ctx.inNoteBody = true;
            ctx.currentNoteId.clear();
            ctx.currentNotePos = normalizeDirection(match.captured(1));
            QString rawTarget = !match.captured(2).isEmpty() ? match.captured(2) : match.captured(3);
            QString targetId, targetMember;
            splitIdAndMember(rawTarget, targetId, targetMember);
            ctx.currentNoteBoundId = targetId;
            ctx.currentNoteBoundMember = targetMember;
            ctx.currentNoteText.clear();
            ctx.noteStartLine = lineNum;
            return true;
        }

        // 7. 单行附加到上一个类的 Note
        match = m_rxSingleAttached.match(line);
        if (match.hasMatch()) {
            NoteDecl note;
            note.position = normalizeDirection(match.captured(1));
            note.text = match.captured(3);
            note.location = SourceLocation{lineNum, 1, (int)line.length()};
            if (!classAst->classes.isEmpty()) {
                note.boundToId = classAst->classes.last().id;
            }
            classAst->notes.append(note);
            return true;
        }

        // 8. 多行附加到上一个类的 Note 开始
        match = m_rxMultiAttachedStart.match(line);
        if (match.hasMatch()) {
            ctx.inNoteBody = true;
            ctx.currentNoteId.clear();
            ctx.currentNoteBoundId.clear();
            ctx.currentNoteBoundMember.clear();
            if (!classAst->classes.isEmpty()) {
                ctx.currentNoteBoundId = classAst->classes.last().id;
            }
            ctx.currentNotePos = normalizeDirection(match.captured(1));
            ctx.currentNoteText.clear();
            ctx.noteStartLine = lineNum;
            return true;
        }

        return false;
    }

private:
    QRegularExpression m_rxSingleFloating;
    QRegularExpression m_rxMultiFloatingStart;
    QRegularExpression m_rxSingleLink;
    QRegularExpression m_rxMultiLinkStart;
    QRegularExpression m_rxSingleBound;
    QRegularExpression m_rxMultiBoundStart;
    QRegularExpression m_rxSingleAttached;
    QRegularExpression m_rxMultiAttachedStart;
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
    PreprocessCommand preprocess;
    TitleCommand title;
    LegendCommand legend;
    ControlCommand control;
    AssociationClassCommand assocClass;
    ExtendsImplementsCommand extendsImpl;
    ClassStyleCommand classStyle;
    TogetherCommand together;
    SkinParamCommand skinParam;
    PackageCommand package;
    ClassDeclCommand classDecl;
    
    StandardRelationCommand stdRelation;
    HyphenRelationCommand hyphenRelation;
    DotRelationCommand dotRelation;
    SpecialRelationCommand specialRelation;

    NoteCommand noteCommand;

    RightBraceCommand rightBrace;
    ParticipantCommand participant;
    MessageCommand message;

    QVector<LineCommand*> commands = {
        &preprocess, &title, &legend, &control, &assocClass, &extendsImpl, &classStyle, &together,
        &skinParam, &package, &classDecl, 
        &stdRelation, &hyphenRelation, &dotRelation, &specialRelation,
        &noteCommand,
        &rightBrace, &participant, &message
    };

    ParserContext ctx;

    // 3. 遍历各行进行模式扫描
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty() || line.startsWith('\'')) {
            result.traceLog.append(QString("Line %1: '%2' -> Skipped (Empty or Comment)").arg(i + 1).arg(line));
            continue;
        }

        // 特殊处理：多行块注释过滤 /' ... '/
        if (ctx.inCommentBlock) {
            result.traceLog.append(QString("Line %1: '%2' -> Skipped (Block Comment)").arg(i + 1).arg(line));
            if (line.contains("'/")) {
                ctx.inCommentBlock = false;
            }
            continue;
        }
        if (line.startsWith("/'")) {
            result.traceLog.append(QString("Line %1: '%2' -> Skipped (Block Comment)").arg(i + 1).arg(line));
            if (!line.contains("'/") || line.indexOf("'/") < line.indexOf("/'")) {
                ctx.inCommentBlock = true;
            }
            continue;
        }

        // 特殊处理：读取多行 Legend 块内部（直接忽略）
        if (ctx.inLegendBody) {
            result.traceLog.append(QString("Line %1: '%2' -> Skipped (Legend Body)").arg(i + 1).arg(line));
            if (line.compare("endlegend", Qt::CaseInsensitive) == 0 || line.compare("end legend", Qt::CaseInsensitive) == 0) {
                ctx.inLegendBody = false;
            }
            continue;
        }

        // 特殊处理：读取多行 Note 体内部
        if (ctx.inNoteBody) {
            result.traceLog.append(QString("Line %1: '%2' -> Inside Note Body").arg(i + 1).arg(line));
            if (line.compare("end note", Qt::CaseInsensitive) == 0) {
                ctx.inNoteBody = false;
                ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(result.ast.get());
                NoteDecl note;
                note.id = ctx.currentNoteId;
                note.boundToId = ctx.currentNoteBoundId;
                note.boundToMember = ctx.currentNoteBoundMember;
                note.position = ctx.currentNotePos;
                note.text = ctx.currentNoteText;
                note.location = SourceLocation{ctx.noteStartLine, 1, (int)line.length()};
                classAst->notes.append(note);
            } else {
                if (!ctx.currentNoteText.isEmpty()) {
                    ctx.currentNoteText += "\n";
                }
                ctx.currentNoteText += line;
            }
            continue;
        }

        // 特殊处理：读取类成员大括号体内部
        if (ctx.inClassBody) {
            result.traceLog.append(QString("Line %1: '%2' -> Inside Class Body").arg(i + 1).arg(line));
            if (line == "}") {
                ctx.inClassBody = false;
                ctx.currentClassId.clear();
            } else {
                ClassDiagramAst* classAst = static_cast<ClassDiagramAst*>(result.ast.get());
                for (auto &c : classAst->classes) {
                    if (c.id == ctx.currentClassId) {
                        c.members.append(parseClassMember(line));
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
                result.traceLog.append(QString("Line %1: '%2' -> Matched by %3").arg(i + 1).arg(line).arg(cmd->name()));
                break;
            }
        }

        if (!matched) {
            result.traceLog.append(QString("Line %1: '%2' -> FAILED to match").arg(i + 1).arg(line));
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
    // 如果这个 ID 已经是一个 Note 的自定义 ID，直接返回，不创建多余类卡片
    for (const auto &note : ast->notes) {
        if (note.id == id) return;
    }
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

QVector<DiagramBlock> splitDiagrams(const QString &sourceText)
{
    QVector<DiagramBlock> blocks;
    QStringList lines = sourceText.split('\n');
    bool inBlock = false;
    int startLineNum = -1;
    QStringList currentLines;
    QString explicitTitle;

    for (int i = 0; i < lines.size(); ++i) {
        QString rawLine = lines[i];
        if (rawLine.endsWith('\r')) {
            rawLine.chop(1);
        }
        QString trimmedLine = rawLine.trimmed();

        if (!inBlock) {
            if (trimmedLine.startsWith("@startuml")) {
                inBlock = true;
                startLineNum = i + 1;
                currentLines.clear();
                currentLines.append(rawLine);

                // 优先级 ①：提取 @startuml 后面的字符
                if (trimmedLine.length() > 9) {
                    QString after = trimmedLine.mid(9).trimmed();
                    if (!after.isEmpty()) {
                        explicitTitle = after;
                    } else {
                        explicitTitle.clear();
                    }
                } else {
                    explicitTitle.clear();
                }
            }
        } else {
            currentLines.append(rawLine);

            if (trimmedLine.startsWith("@enduml")) {
                inBlock = false;

                // 决定标题
                QString finalTitle = explicitTitle;
                // 优先级 ②：若无，前瞻寻找该块首行是否有 title 指令
                if (finalTitle.isEmpty()) {
                    QRegularExpression titleRx("^\\s*title\\s+(.+)$", QRegularExpression::CaseInsensitiveOption);
                    for (int j = 1; j < currentLines.size() - 1; ++j) {
                        QString blockLine = currentLines[j].trimmed();
                        auto match = titleRx.match(blockLine);
                        if (match.hasMatch()) {
                            QString captured = match.captured(1).trimmed();
                            if (!captured.isEmpty()) {
                                finalTitle = captured;
                                break;
                            }
                        }
                    }
                }

                // 优先级 ③：若依然没有，默认标题为 Diagram <1-indexed 序号>
                if (finalTitle.isEmpty()) {
                    finalTitle = QString("Diagram %1").arg(blocks.size() + 1);
                }

                DiagramBlock block;
                block.title = finalTitle;
                block.content = currentLines.join('\n');
                block.startLine = startLineNum;
                blocks.append(block);
            }
        }
    }

    // 容错：如果文件结束了但 inBlock 还为 true
    if (inBlock && !currentLines.isEmpty()) {
        QString finalTitle = explicitTitle;
        if (finalTitle.isEmpty()) {
            QRegularExpression titleRx("^\\s*title\\s+(.+)$", QRegularExpression::CaseInsensitiveOption);
            for (int j = 1; j < currentLines.size(); ++j) {
                QString blockLine = currentLines[j].trimmed();
                auto match = titleRx.match(blockLine);
                if (match.hasMatch()) {
                    QString captured = match.captured(1).trimmed();
                    if (!captured.isEmpty()) {
                        finalTitle = captured;
                        break;
                    }
                }
            }
        }
        if (finalTitle.isEmpty()) {
            finalTitle = QString("Diagram %1").arg(blocks.size() + 1);
        }

        DiagramBlock block;
        block.title = finalTitle;
        block.content = currentLines.join('\n');
        block.startLine = startLineNum;
        blocks.append(block);
    }

    return blocks;
}

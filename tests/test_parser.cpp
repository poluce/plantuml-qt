#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "../src/parser/PumlParser.h"
#include "../src/ast/DiagramAst.h"
#include "../src/graphviz/GraphvizLayoutEngine.h"
#include "../src/render_model/RenderTheme.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString filePath = "class_diagram.puml";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        file.setFileName("../class_diagram.puml");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCritical() << "Cannot open class_diagram.puml!";
            return 1;
        }
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    qDebug() << "Loaded class_diagram.puml successfully. Length:" << content.length();

    PumlParser parser(content);
    ParseResult result = parser.parse();

    if (result.ast) {
        qDebug() << "Is Class Diagram:" << (!result.ast->isSequence());
    } else {
        qDebug() << "AST is null!";
    }
    qDebug() << "Error count:" << result.errors.size();

    for (const auto &err : result.errors) {
        qWarning() << "Line" << err.location.line << ":" << err.message;
    }

    // ================= 增加：测试新支持的高级类图语法 =================
    // ================= 增加：测试新支持的高级类图语法 =================
    QString advancedPuml = 
        "@startuml\n"
        "!pragma useVerticalIf on\n"
        "/'\n"
        "  这是一个多行块注释测试\n"
        "  它不应该影响解析并保持行号对齐\n"
        "'/\n"
        "hide empty members\n"
        "remove NonExistentClass\n"
        "struct StructA {\n"
        "  field : int\n"
        "}\n"
        "protocol ProtocolB\n"
        "exception ExceptionC\n"
        "metaclass MetaclassD\n"
        "circle CircleNode\n"
        "() InterfaceNode\n"
        "diamond DiamondNode\n"
        "<> DiamondNode2\n"
        "json JsonCard {\n"
        "  \"key\": \"value\",\n"
        "  \"array\": [1, 2, 3]\n"
        "}\n"
        "package MyPackage #lightgray {\n"
        "  class ClassStyled #red;line:blue;line.dotted;text:green\n"
        "}\n"
        "class \"My Long Class Name\" as LongClass <<MyStereotype>> #ffeedd {\n"
        "  + {static} staticField : int\n"
        "  - {abstract} abstractMethod() : void\n"
        "  -- 属性分隔线 --\n"
        "  ~ packageField\n"
        "}\n"
        "LongClass : +externalMethod() : void\n"
        "class TargetClass\n"
        "LongClass \"1\" *-- \"many\" TargetClass : aggregation\n"
        "LongClass +- NestedClass\n"
        "LongClass -[#red,dashed,thickness=4]-> TargetClass : styledRelation\n"
        "StructA #-- ProtocolB\n"
        "ProtocolB x-- ExceptionC\n"
        "ExceptionC }-- MetaclassD\n"
        "MetaclassD ^-- LongClass\n"
        "StructA --^ TargetClass\n"
        "StructA --> ProtocolB #line:blue;line.bold;text:red : labelWithTrailingStyle\n"
        "note \"This is a floating note\" as floating_note1\n"
        "note as floating_note2\n"
        "  Line 1 of floating note\n"
        "  Line 2 of floating note\n"
        "end note\n"
        "note on link #red : This is single line note on link\n"
        "note left on link\n"
        "  Line 1 of multi-line note on link\n"
        "  Line 2 of multi-line note on link\n"
        "end note\n"
        "note left of LongClass\n"
        "  这是第一行 note\n"
        "  这是第二行 note\n"
        "end note\n"
        "StructA::field --> ProtocolB::field\n"
        "StructA [限定符1] --> ProtocolB [限定符2]\n"
        "hide TargetClass\n"
        "remove ExceptionC\n"
        "(StructA, ProtocolB) .. TargetClass\n"
        "@enduml";

    qDebug() << "\n==== 测试高级语法解析 ====";
    PumlParser advParser(advancedPuml);
    ParseResult advResult = advParser.parse();
    qDebug() << "解析错误数:" << advResult.errors.size();
    for (const auto &err : advResult.errors) {
        qWarning() << "  Line" << err.location.line << ":" << err.message;
    }

    bool hasErrors = !result.errors.isEmpty() || !advResult.errors.isEmpty();

    if (advResult.ast && !advResult.ast->isSequence()) {
        auto *classAst = static_cast<ClassDiagramAst*>(advResult.ast.get());
        qDebug() << "解析到的类数量:" << classAst->classes.size();
        for (const auto &c : classAst->classes) {
            qDebug() << "  类 ID:" << c.id << "显示名:" << c.displayName << "元类型:" << c.metaType << "Stereotype:" << c.stereotype << "样式:" << c.style;
            qDebug() << "  成员数:" << c.members.size();
            for (const auto &m : c.members) {
                qDebug() << "    成员文本:" << m.rawText << "可见性:" << m.visibility << "static:" << m.isStatic << "abstract:" << m.isAbstract << "separator:" << m.isSeparator << "sepText:" << m.separatorText;
            }
        }
        qDebug() << "解析到的连线数量:" << classAst->relations.size();
        for (const auto &r : classAst->relations) {
            qDebug() << "  连线:" << r.from << (r.fromMember.isEmpty() ? "" : "::" + r.fromMember)
                     << "[\"" << r.fromMultiplicity << "\"] -> "
                     << r.to << (r.toMember.isEmpty() ? "" : "::" + r.toMember)
                     << "[\"" << r.toMultiplicity << "\"] 类别:" << (int)r.kind << "文本:" << r.text << "样式:" << r.style;
        }
        qDebug() << "解析到的 Note 数量:" << classAst->notes.size();
        for (const auto &n : classAst->notes) {
            qDebug() << "  Note ID:" << n.id << "绑定到:" << n.boundToId << "方向:" << n.position << "文本:" << n.text;
        }
        qDebug() << "解析到的隐藏元素列表 (hiddenElements):" << classAst->hiddenElements;
        qDebug() << "解析到的删除元素列表 (removedElements):" << classAst->removedElements;
        qDebug() << "解析到的关系类数量:" << classAst->associationClasses.size();
        for (const auto &ac : classAst->associationClasses) {
            qDebug() << "  关系类: (" << ac.classA << "," << ac.classB << ") ->" << ac.assocClass;
        }

        // 调用布局引擎生成 DOT 源码并测试
        qDebug() << "\n==== 执行 Layout 并打印 DOT 源码 ====";
        GraphvizLayoutEngine engine;
        engine.layout(*advResult.ast, RenderTheme());
    }

    return hasErrors ? 1 : 0;
}

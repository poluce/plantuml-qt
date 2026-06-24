#include <QGuiApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "../src/parser/PumlParser.h"
#include "../src/ast/DiagramAst.h"
#include "../src/graphviz/GraphvizLayoutEngine.h"
#include "../src/render_model/RenderTheme.h"

// 自定义强断言宏，确保无论在 Debug 还是 Release 模式下均保持活性
#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            qFatal("Test assertion failed: %s, file %s, line %d", #cond, __FILE__, __LINE__); \
        } \
    } while (0)

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    
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

        // ================= 新增：对 Milestone 1 核心要求的自动化断言校验 =================
        qDebug() << "\n==== 执行 Milestone 1 自动化断言校验 ====";

        // 验证没有 "ProtocolB:red" 且复合行尾样式解析正确，无冒号截断
        {
            bool hasProtocolB = false;
            bool hasProtocolBRed = false;
            for (const auto &c : classAst->classes) {
                if (c.id == "ProtocolB") hasProtocolB = true;
                if (c.id.contains("ProtocolB:red") || c.id.contains(":red")) hasProtocolBRed = true;
            }
            ASSERT_TRUE(hasProtocolB);
            ASSERT_TRUE(!hasProtocolBRed);

            bool hasCorrectTrailingStyle = false;
            for (const auto &r : classAst->relations) {
                if (r.text == "labelWithTrailingStyle") {
                    ASSERT_TRUE(r.style == "line:blue;line.bold;text:red");
                    hasCorrectTrailingStyle = true;
                }
            }
            ASSERT_TRUE(hasCorrectTrailingStyle);

            // 验证嵌套连线 (LongClass +- NestedClass) 被解析为 RelationKind::Nested
            bool hasNestedRelation = false;
            for (const auto &r : classAst->relations) {
                if (r.from == "LongClass" && r.to == "NestedClass") {
                    ASSERT_TRUE(r.kind == RelationKind::Nested);
                    hasNestedRelation = true;
                }
            }
            ASSERT_TRUE(hasNestedRelation);

            // 验证 Square, Cross, Crowfoot, Hat 特殊关系类型的解析
            bool hasSquare = false, hasCross = false, hasCrowfoot = false, hasHat = false;
            for (const auto &r : classAst->relations) {
                if (r.from == "StructA" && r.to == "ProtocolB") {
                    ASSERT_TRUE(r.kind == RelationKind::Square);
                    hasSquare = true;
                }
                if (r.from == "ProtocolB" && r.to == "ExceptionC") {
                    ASSERT_TRUE(r.kind == RelationKind::Cross);
                    hasCross = true;
                }
                if (r.from == "ExceptionC" && r.to == "MetaclassD") {
                    ASSERT_TRUE(r.kind == RelationKind::Crowfoot);
                    hasCrowfoot = true;
                }
                if (r.from == "MetaclassD" && r.to == "LongClass") {
                    ASSERT_TRUE(r.kind == RelationKind::Hat);
                    hasHat = true;
                }
            }
            ASSERT_TRUE(hasSquare);
            ASSERT_TRUE(hasCross);
            ASSERT_TRUE(hasCrowfoot);
            ASSERT_TRUE(hasHat);
        }

        // 验证多行 Note 物理行列及长度精确对齐断言
        {
            bool hasFloatingNote2 = false;
            for (const auto &n : classAst->notes) {
                if (n.id == "floating_note2") {
                    hasFloatingNote2 = true;
                    ASSERT_TRUE(n.location.line == 44);
                    ASSERT_TRUE(n.location.column == 1);
                    ASSERT_TRUE(n.location.length == 22);
                }
            }
            ASSERT_TRUE(hasFloatingNote2);
        }
        
        // 1. 空格泛型及去泛型联动测试
        QString genericsPuml = 
            "@startuml\n"
            "class \"Map<K, V>\" as MyMap\n"
            "class List<T>\n"
            "MyMap --> List : uses\n"
            "List<T> <|-- ArrayList\n" // 连线端点带泛型
            "@enduml";
        PumlParser genParser(genericsPuml);
        ParseResult genResult = genParser.parse();
        ASSERT_TRUE(genResult.errors.isEmpty());
        if (genResult.ast && !genResult.ast->isSequence()) {
            auto *genAst = static_cast<ClassDiagramAst*>(genResult.ast.get());
            ASSERT_TRUE(genAst->classes.size() == 3);
            
            const ClassDecl* mapDecl = nullptr;
            const ClassDecl* listDecl = nullptr;
            for (const auto &c : genAst->classes) {
                if (c.id == "MyMap") mapDecl = &c;
                if (c.id == "List") listDecl = &c;
            }
            ASSERT_TRUE(mapDecl != nullptr);
            ASSERT_TRUE(mapDecl->generics == "K, V");
            ASSERT_TRUE(mapDecl->displayName == "Map");
            
            ASSERT_TRUE(listDecl != nullptr);
            ASSERT_TRUE(listDecl->generics == "T");
            ASSERT_TRUE(listDecl->displayName == "List");
            
            ASSERT_TRUE(genAst->relations.size() == 2);
            qDebug() << "genAst->relations[0].from =" << genAst->relations[0].from << "to =" << genAst->relations[0].to;
            ASSERT_TRUE(genAst->relations[0].from == "List");
            ASSERT_TRUE(genAst->relations[0].to == "MyMap");
            ASSERT_TRUE(genAst->relations[1].from == "List");
            ASSERT_TRUE(genAst->relations[1].to == "ArrayList");
        }

        // 2. 带引号与不带引号的多重度及连线方向、样式测试
        QString multiPuml = 
            "@startuml\n"
            "ClassA \"1\" *-- \"many\" ClassB : stdMultiplicity\n"
            "ClassA 1 *-- * ClassB : nonQuoteMultiplicity\n"
            "ClassA 0..1 -- 1..* ClassB : rangeMultiplicity\n"
            "ClassA *-- ClassB : compositionConnection\n"
            "ClassA \"1\" -[#blue,dashed,up]-> \"0..1\" ClassB : styledInlineDirection\n"
            "ClassA -down-> ClassB #line:red;line.bold : tailStyle\n"
            "@enduml";
        PumlParser multiParser(multiPuml);
        ParseResult multiResult = multiParser.parse();
        ASSERT_TRUE(multiResult.errors.isEmpty());
        if (multiResult.ast && !multiResult.ast->isSequence()) {
            auto *mAst = static_cast<ClassDiagramAst*>(multiResult.ast.get());
            ASSERT_TRUE(mAst->relations.size() == 6);
            for (const auto &r : mAst->relations) {
                if (r.text == "stdMultiplicity") {
                    ASSERT_TRUE(r.fromMultiplicity == "1");
                    ASSERT_TRUE(r.toMultiplicity == "many");
                    ASSERT_TRUE(r.kind == RelationKind::Composition);
                } else if (r.text == "nonQuoteMultiplicity") {
                    ASSERT_TRUE(r.fromMultiplicity == "1");
                    ASSERT_TRUE(r.toMultiplicity == "*");
                    ASSERT_TRUE(r.kind == RelationKind::Composition);
                } else if (r.text == "rangeMultiplicity") {
                    ASSERT_TRUE(r.fromMultiplicity == "0..1");
                    ASSERT_TRUE(r.toMultiplicity == "1..*");
                    ASSERT_TRUE(r.kind == RelationKind::AssociationLine);
                } else if (r.text == "compositionConnection") {
                    ASSERT_TRUE(r.fromMultiplicity.isEmpty());
                    ASSERT_TRUE(r.toMultiplicity.isEmpty());
                    ASSERT_TRUE(r.kind == RelationKind::Composition);
                } else if (r.text == "styledInlineDirection") {
                    ASSERT_TRUE(r.fromMultiplicity == "0..1");
                    ASSERT_TRUE(r.toMultiplicity == "1");
                    ASSERT_TRUE(r.direction == "down");
                    ASSERT_TRUE(r.style == "#blue,dashed");
                }
            }
        }

        // 3. 精准列号与长度对齐测试
        QString positionPuml = 
            "@startuml\n"
            "  class A\n"
            "  invalid_syntax_here\n"
            "@enduml";
        PumlParser posParser(positionPuml);
        ParseResult posResult = posParser.parse();
        ASSERT_TRUE(!posResult.errors.isEmpty());
        ASSERT_TRUE(posResult.errors[0].location.line == 3);
        ASSERT_TRUE(posResult.errors[0].location.column == 3);
        ASSERT_TRUE(posResult.errors[0].location.length == 19);

        // 4. 未闭合结构报错与块注释正确闭合测试
        QString unclosedPuml = 
            "@startuml\n"
            "class A {\n"
            "  member : int\n"
            "@enduml";
        PumlParser unclosedParser(unclosedPuml);
        ParseResult unclosedResult = unclosedParser.parse();
        ASSERT_TRUE(!unclosedResult.errors.isEmpty());
        ASSERT_TRUE(unclosedResult.errors[0].message.contains("未闭合") || unclosedResult.errors[0].message.contains("}"));

        QString unclosedCommentPuml = 
            "@startuml\n"
            "/'\n"
            "这是一个未闭合的块注释\n"
            "@enduml";
        PumlParser unclosedCommentParser(unclosedCommentPuml);
        ParseResult unclosedCommentResult = unclosedCommentParser.parse();
        ASSERT_TRUE(!unclosedCommentResult.errors.isEmpty());
        ASSERT_TRUE(unclosedCommentResult.errors[0].message.contains("块注释") || unclosedCommentResult.errors[0].message.contains("未闭合"));

        // 5. 特殊右向关联连线测试 (如 A -up-* B, C -left-o D)
        QString specialRightPuml = 
            "@startuml\n"
            "A -up-* B\n"
            "C -left-o D\n"
            "@enduml";
        PumlParser specRightParser(specialRightPuml);
        ParseResult specRightResult = specRightParser.parse();
        ASSERT_TRUE(specRightResult.errors.isEmpty());
        if (specRightResult.ast && !specRightResult.ast->isSequence()) {
            auto *srAst = static_cast<ClassDiagramAst*>(specRightResult.ast.get());
            ASSERT_TRUE(srAst->relations.size() == 2);
            
            const RelationDecl* relAB = nullptr;
            const RelationDecl* relCD = nullptr;
            for (const auto &r : srAst->relations) {
                if (r.from == "A" && r.to == "B") relAB = &r;
                if (r.from == "C" && r.to == "D") relCD = &r;
            }
            ASSERT_TRUE(relAB != nullptr);
            ASSERT_TRUE(relAB->kind == RelationKind::Composition);
            ASSERT_TRUE(relAB->direction == "up");
            
            ASSERT_TRUE(relCD != nullptr);
            ASSERT_TRUE(relCD->kind == RelationKind::Aggregation);
            ASSERT_TRUE(relCD->direction == "left");
        }

        qDebug() << "Milestone 1 自动化断言校验成功通过！";

        // ================= 新增：对 Milestone 2 核心要求的自动化断言校验 =================
        qDebug() << "\n==== 执行 Milestone 2 自动化断言校验 ====";

        // 测试用例 1：多层嵌套包的 AST 解析与上下文类查找断言
        QString m2Puml =
            "@startuml\n"
            "package ParentPkg {\n"
            "    package ChildPkg {\n"
            "        class ClassInside\n"
            "    }\n"
            "    class ClassMid\n"
            "}\n"
            "\n"
            "ParentPkg.ChildPkg.ClassInside <-- ClassMid : path_ref1\n"
            "ParentPkg::ChildPkg::ClassInside <-- ClassOutside : path_ref2\n"
            "ClassInside <-- ClassMid : relative_ref\n"
            "@enduml";

        PumlParser m2Parser(m2Puml);
        ParseResult m2Result = m2Parser.parse();
        ASSERT_TRUE(m2Result.errors.isEmpty());
        if (m2Result.ast && !m2Result.ast->isSequence()) {
            auto *classAst = static_cast<ClassDiagramAst*>(m2Result.ast.get());
            
            // 验证已声明类的 ID 被层次化归一化
            bool hasInside = false;
            bool hasMid = false;
            bool hasOutside = false;
            for (const auto &c : classAst->classes) {
                if (c.id == "ParentPkg.ChildPkg.ClassInside") {
                    hasInside = true;
                    ASSERT_TRUE(c.displayName == "ClassInside");
                    ASSERT_TRUE(c.packageName == "ParentPkg.ChildPkg");
                }
                if (c.id == "ParentPkg.ClassMid") {
                    hasMid = true;
                    ASSERT_TRUE(c.displayName == "ClassMid");
                    ASSERT_TRUE(c.packageName == "ParentPkg");
                }
                if (c.id == "ClassOutside") {
                    hasOutside = true;
                    ASSERT_TRUE(c.displayName == "ClassOutside");
                    ASSERT_TRUE(c.packageName.isEmpty());
                }
            }
            ASSERT_TRUE(hasInside);
            ASSERT_TRUE(hasMid);
            ASSERT_TRUE(hasOutside);

            // 验证 relations 的端点解析与上下文查找
            const RelationDecl* pathRef1 = nullptr;
            const RelationDecl* pathRef2 = nullptr;
            const RelationDecl* relativeRef = nullptr;
            for (const auto &r : classAst->relations) {
                if (r.text == "path_ref1") pathRef1 = &r;
                if (r.text == "path_ref2") pathRef2 = &r;
                if (r.text == "relative_ref") relativeRef = &r;
            }

            ASSERT_TRUE(pathRef1 != nullptr);
            ASSERT_TRUE(pathRef1->from == "ParentPkg.ChildPkg.ClassInside");
            ASSERT_TRUE(pathRef1->to == "ParentPkg.ClassMid");

            ASSERT_TRUE(pathRef2 != nullptr);
            ASSERT_TRUE(pathRef2->from == "ParentPkg.ChildPkg.ClassInside");
            ASSERT_TRUE(pathRef2->to == "ClassOutside");

            ASSERT_TRUE(relativeRef != nullptr);
            ASSERT_TRUE(relativeRef->from == "ParentPkg.ChildPkg.ClassInside");
            ASSERT_TRUE(relativeRef->to == "ParentPkg.ClassMid");

            GraphvizLayoutEngine engine;
            LayoutGraph graph = engine.buildClassGraph(static_cast<const ClassDiagramAst&>(*m2Result.ast));
            DotBuilder builder;
            QString dotSource = builder.build(graph);
            
            qDebug() << "Generated Dot Source for M2:\n" << dotSource;

            // 正则验证嵌套 subgraph 结构
            QRegularExpression nestedClusterRx(
                "subgraph\\s+\"cluster_ParentPkg\"\\s*\\{[\\s\\S]*subgraph\\s+\"cluster_ParentPkg\\.ChildPkg\"\\s*\\{"
            );
            ASSERT_TRUE(nestedClusterRx.match(dotSource).hasMatch());

            // 验证节点在正确的包里输出
            ASSERT_TRUE(dotSource.contains("\"ParentPkg.ChildPkg.ClassInside\""));
            ASSERT_TRUE(dotSource.contains("\"ParentPkg.ClassMid\""));
        }

        // 测试用例 2：自定义线段属性转译与覆盖校验
        QString stylePuml =
            "@startuml\n"
            "ClassA -[#red,dashed,thickness=4]-> ClassB : style_inline\n"
            "ClassA --> ClassB #line:blue;line.bold;text:green : style_trailing\n"
            "ClassA -[dotted]-> ClassB : dotted_inline\n"
            "@enduml";

        PumlParser styleParser(stylePuml);
        ParseResult styleResult = styleParser.parse();
        for (const auto &err : styleResult.errors) {
            qWarning() << "styleResult error: Line" << err.location.line << ":" << err.message;
        }
        ASSERT_TRUE(styleResult.errors.isEmpty());
        if (styleResult.ast && !styleResult.ast->isSequence()) {
            GraphvizLayoutEngine engine;
            LayoutGraph graph = engine.buildClassGraph(static_cast<const ClassDiagramAst&>(*styleResult.ast));
            DotBuilder builder;
            QString dotSource = builder.build(graph);

            qDebug() << "Generated Dot Source for style:\n" << dotSource;

            // 验证 inline style: color="#red", style="dashed", penwidth="4"
            QRegularExpression inlineEdgeRx(
                "\"ClassB\"\\s*->\\s*\"ClassA\"\\s*\\[(?=[^\\]]*color=\"#red\")(?=[^\\]]*style=\"dashed\")(?=[^\\]]*penwidth=\"4\")[^\\]]*\\]"
            );
            ASSERT_TRUE(inlineEdgeRx.match(dotSource).hasMatch());

            // 验证 trailing style: color="blue", penwidth="2.5", fontcolor="green"
            QRegularExpression trailingEdgeRx(
                "\"ClassB\"\\s*->\\s*\"ClassA\"\\s*\\[(?=[^\\]]*color=\"blue\")(?=[^\\]]*fontcolor=\"green\")(?=[^\\]]*penwidth=\"2.5\")[^\\]]*\\]"
            );
            ASSERT_TRUE(trailingEdgeRx.match(dotSource).hasMatch());

            // 验证简写 dotted 样式: style="dotted"
            QRegularExpression dottedEdgeRx(
                "\"ClassB\"\\s*->\\s*\"ClassA\"\\s*\\[(?=[^\\]]*style=\"dotted\")[^\\]]*\\]"
            );
            ASSERT_TRUE(dottedEdgeRx.match(dotSource).hasMatch());
        }

        qDebug() << "Milestone 2 自动化断言校验成功通过！";

        // ================= 新增：对 Milestone 3 核心要求的自动化断言校验 =================
        qDebug() << "\n==== 执行 Milestone 3 自动化断言校验 ====";

        {
            // 1. 验证 LongClass 成员属性解析 (可见性图标、static/abstract修饰、属性分隔线等)
            const ClassDecl* longClassDecl = nullptr;
            for (const auto &c : classAst->classes) {
                if (c.id == "LongClass") {
                    longClassDecl = &c;
                    break;
                }
            }
            ASSERT_TRUE(longClassDecl != nullptr);
            ASSERT_TRUE(longClassDecl->displayName == "My Long Class Name");
            ASSERT_TRUE(longClassDecl->stereotype == "MyStereotype");
            ASSERT_TRUE(longClassDecl->style == "back:#ffeedd");

            // LongClass 共有 5 个成员 (类体内 4 个，外部追加 1 个)
            ASSERT_TRUE(longClassDecl->members.size() == 5);

            // 成员 1: + {static} staticField : int
            const auto &m1 = longClassDecl->members[0];
            ASSERT_TRUE(m1.rawText == "+ {static} staticField : int");
            ASSERT_TRUE(m1.visibility == "+");
            ASSERT_TRUE(m1.isStatic == true);
            ASSERT_TRUE(m1.isAbstract == false);
            ASSERT_TRUE(m1.isSeparator == false);
            ASSERT_TRUE(m1.cleanText == "staticField : int");

            // 成员 2: - {abstract} abstractMethod() : void
            const auto &m2 = longClassDecl->members[1];
            ASSERT_TRUE(m2.rawText == "- {abstract} abstractMethod() : void");
            ASSERT_TRUE(m2.visibility == "-");
            ASSERT_TRUE(m2.isStatic == false);
            ASSERT_TRUE(m2.isAbstract == true);
            ASSERT_TRUE(m2.isSeparator == false);
            ASSERT_TRUE(m2.cleanText == "abstractMethod() : void");

            // 成员 3: -- 属性分隔线 --
            const auto &m3 = longClassDecl->members[2];
            ASSERT_TRUE(m3.rawText == "-- 属性分隔线 --");
            ASSERT_TRUE(m3.visibility == "");
            ASSERT_TRUE(m3.isStatic == false);
            ASSERT_TRUE(m3.isAbstract == false);
            ASSERT_TRUE(m3.isSeparator == true);
            ASSERT_TRUE(m3.separatorText == "属性分隔线");

            // 成员 4: ~ packageField
            const auto &m4 = longClassDecl->members[3];
            ASSERT_TRUE(m4.rawText == "~ packageField");
            ASSERT_TRUE(m4.visibility == "~");
            ASSERT_TRUE(m4.isStatic == false);
            ASSERT_TRUE(m4.isAbstract == false);
            ASSERT_TRUE(m4.isSeparator == false);
            ASSERT_TRUE(m4.cleanText == "packageField");

            // 成员 5 (外部追加): +externalMethod() : void
            const auto &m5 = longClassDecl->members[4];
            ASSERT_TRUE(m5.rawText == "+externalMethod() : void");
            ASSERT_TRUE(m5.visibility == "+");
            ASSERT_TRUE(m5.isStatic == false);
            ASSERT_TRUE(m5.isAbstract == false);
            ASSERT_TRUE(m5.isSeparator == false);
            ASSERT_TRUE(m5.cleanText == "externalMethod() : void");
        }

        {
            // 2. 验证 RelationDecl 属性解析 (连线中括号样式与两端多重度文本)
            const RelationDecl* relAggr = nullptr;
            const RelationDecl* relStyled = nullptr;
            const RelationDecl* relNested = nullptr;

            for (const auto &r : classAst->relations) {
                if (r.text == "aggregation") {
                    relAggr = &r;
                } else if (r.text == "styledRelation") {
                    relStyled = &r;
                } else if (r.from == "LongClass" && r.to == "NestedClass" && r.kind == RelationKind::Nested) {
                    relNested = &r;
                }
            }

            ASSERT_TRUE(relAggr != nullptr);
            ASSERT_TRUE(relAggr->from == "LongClass");
            ASSERT_TRUE(relAggr->to == "TargetClass");
            ASSERT_TRUE(relAggr->fromMultiplicity == "1");
            ASSERT_TRUE(relAggr->toMultiplicity == "many");
            ASSERT_TRUE(relAggr->kind == RelationKind::Composition);

            ASSERT_TRUE(relStyled != nullptr);
            // 经过右向关系对调归一化后，from 和 to 发生了对调，并且 direction 也被 flip
            ASSERT_TRUE(relStyled->from == "TargetClass");
            ASSERT_TRUE(relStyled->to == "LongClass");
            ASSERT_TRUE(relStyled->style == "#red,dashed,thickness=4");
            ASSERT_TRUE(relStyled->kind == RelationKind::Association);

            ASSERT_TRUE(relNested != nullptr);
            ASSERT_TRUE(relNested->from == "LongClass");
            ASSERT_TRUE(relNested->to == "NestedClass");
            ASSERT_TRUE(relNested->kind == RelationKind::Nested);
        }

        qDebug() << "Milestone 3 自动化断言校验成功通过！";
    }

    // ================= 新增：时序图被误判为类图的回归测试 =================
    {
        QString seqPuml =
            "@startuml\n"
            "Alice -> Bob : hello\n"
            "Bob --> Alice : response\n"
            "@enduml";

        PumlParser seqParser(seqPuml);
        ParseResult seqResult = seqParser.parse();
        for (const auto &err : seqResult.errors) {
            qWarning() << "seqResult error: Line" << err.location.line << ":" << err.message;
        }
        ASSERT_TRUE(seqResult.errors.isEmpty());
        ASSERT_TRUE(seqResult.ast != nullptr);
        ASSERT_TRUE(seqResult.ast->isSequence());
        qDebug() << "时序图回归测试校验成功通过！";
    }

    // ================= 新增：测试用例A - 包含带颜色/虚线等样式的时序图 =================
    {
        QString styleSeqPuml =
            "@startuml\n"
            "Alice -[#red]-> Bob : hello\n"
            "Bob ..[#blue]> Alice : ok\n"
            "@enduml";

        PumlParser styleSeqParser(styleSeqPuml);
        ParseResult styleSeqResult = styleSeqParser.parse();
        for (const auto &err : styleSeqResult.errors) {
            qWarning() << "styleSeqResult error: Line" << err.location.line << ":" << err.message;
        }
        ASSERT_TRUE(styleSeqResult.errors.isEmpty());
        ASSERT_TRUE(styleSeqResult.ast != nullptr);
        ASSERT_TRUE(styleSeqResult.ast->isSequence());
        qDebug() << "测试用例A：带样式时序图回归测试成功通过！";
    }

    // ================= 新增：测试用例B - 仅包含嵌套连线关系的类图 =================
    {
        QString nestedClassPuml =
            "@startuml\n"
            "ClassA +-- ClassB\n"
            "ClassC +.. ClassD\n"
            "@enduml";

        PumlParser nestedClassParser(nestedClassPuml);
        ParseResult nestedClassResult = nestedClassParser.parse();
        for (const auto &err : nestedClassResult.errors) {
            qWarning() << "nestedClassResult error: Line" << err.location.line << ":" << err.message;
        }
        ASSERT_TRUE(nestedClassResult.errors.isEmpty());
        ASSERT_TRUE(nestedClassResult.ast != nullptr);
        ASSERT_TRUE(!nestedClassResult.ast->isSequence());

        auto *ncAst = static_cast<ClassDiagramAst*>(nestedClassResult.ast.get());
        ASSERT_TRUE(ncAst->relations.size() == 2);
        bool hasAB = false;
        bool hasCD = false;
        for (const auto &r : ncAst->relations) {
            if (r.from == "ClassA" && r.to == "ClassB") {
                ASSERT_TRUE(r.kind == RelationKind::Nested);
                hasAB = true;
            }
            if (r.from == "ClassC" && r.to == "ClassD") {
                ASSERT_TRUE(r.kind == RelationKind::Nested);
                hasCD = true;
            }
        }
        ASSERT_TRUE(hasAB);
        ASSERT_TRUE(hasCD);

        qDebug() << "测试用例B：纯嵌套关系类图回归测试成功通过！";
    }

    return hasErrors ? 1 : 0;
}

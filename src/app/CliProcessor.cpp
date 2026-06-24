#include "CliProcessor.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QImage>
#include "../mainwindow.h"
#include "../parser/PumlParser.h"
#include "../graphviz/GraphvizLayoutEngine.h"
#include "../render_model/RenderTheme.h"
#include "../graphics/DiagramScene.h"
#include "../graphics/DiagramSceneRenderer.h"
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace {
    QJsonObject serializeLocation(const SourceLocation &loc) {
        QJsonObject obj;
        obj["line"] = loc.line;
        obj["column"] = loc.column;
        obj["length"] = loc.length;
        return obj;
    }

    QJsonObject serializeClassMember(const ClassMember &m) {
        QJsonObject obj;
        obj["rawText"] = m.rawText;
        obj["visibility"] = m.visibility;
        obj["isStatic"] = m.isStatic;
        obj["isAbstract"] = m.isAbstract;
        obj["isSeparator"] = m.isSeparator;
        obj["separatorText"] = m.separatorText;
        return obj;
    }

    QJsonObject serializeAst(const DiagramAst &ast) {
        QJsonObject root;
        if (ast.isSequence()) {
            root["type"] = "sequence";
            const auto &seq = static_cast<const SequenceDiagramAst&>(ast);
            
            QJsonArray partsArr;
            for (const auto &p : seq.participants) {
                QJsonObject pObj;
                pObj["id"] = p.id;
                pObj["displayName"] = p.displayName;
                pObj["location"] = serializeLocation(p.location);
                partsArr.append(pObj);
            }
            root["participants"] = partsArr;
            
            QJsonArray stmtsArr;
            for (const auto &s : seq.statements) {
                QJsonObject sObj;
                sObj["from"] = s.from;
                sObj["to"] = s.to;
                sObj["text"] = s.text;
                sObj["kind"] = s.kind == MessageKind::Sync ? "Sync" : "Reply";
                sObj["location"] = serializeLocation(s.location);
                stmtsArr.append(sObj);
            }
            root["statements"] = stmtsArr;
        } else {
            root["type"] = "class";
            const auto &cls = static_cast<const ClassDiagramAst&>(ast);
            root["leftToRight"] = cls.leftToRight;
            
            QJsonArray classesArr;
            for (const auto &c : cls.classes) {
                QJsonObject cObj;
                cObj["id"] = c.id;
                cObj["displayName"] = c.displayName;
                cObj["metaType"] = c.metaType;
                cObj["packageName"] = c.packageName;
                cObj["stereotype"] = c.stereotype;
                cObj["style"] = c.style;
                cObj["location"] = serializeLocation(c.location);
                
                QJsonArray memsArr;
                for (const auto &m : c.members) {
                    memsArr.append(serializeClassMember(m));
                }
                cObj["members"] = memsArr;
                classesArr.append(cObj);
            }
            root["classes"] = classesArr;
            
            QJsonArray relsArr;
            for (const auto &r : cls.relations) {
                QJsonObject rObj;
                rObj["from"] = r.from;
                rObj["to"] = r.to;
                rObj["fromMember"] = r.fromMember;
                rObj["toMember"] = r.toMember;
                rObj["fromMultiplicity"] = r.fromMultiplicity;
                rObj["toMultiplicity"] = r.toMultiplicity;
                rObj["fromQualifier"] = r.fromQualifier;
                rObj["toQualifier"] = r.toQualifier;
                rObj["text"] = r.text;
                rObj["direction"] = r.direction;
                rObj["style"] = r.style;
                rObj["location"] = serializeLocation(r.location);
                
                QString rKindStr;
                switch (r.kind) {
                    case RelationKind::Inheritance: rKindStr = "Inheritance"; break;
                    case RelationKind::Composition: rKindStr = "Composition"; break;
                    case RelationKind::Aggregation: rKindStr = "Aggregation"; break;
                    case RelationKind::Realization: rKindStr = "Realization"; break;
                    case RelationKind::Dependency:  rKindStr = "Dependency"; break;
                    case RelationKind::Association: rKindStr = "Association"; break;
                    case RelationKind::Nested:      rKindStr = "Nested"; break;
                    case RelationKind::AssociationLine: rKindStr = "AssociationLine"; break;
                    case RelationKind::Square:      rKindStr = "Square"; break;
                    case RelationKind::Cross:       rKindStr = "Cross"; break;
                    case RelationKind::Crowfoot:    rKindStr = "Crowfoot"; break;
                    case RelationKind::Hat:         rKindStr = "Hat"; break;
                }
                rObj["kind"] = rKindStr;
                relsArr.append(rObj);
            }
            root["relations"] = relsArr;
            
            QJsonArray pkgsArr;
            for (const auto &p : cls.packages) {
                QJsonObject pObj;
                pObj["id"] = p.id;
                pObj["displayName"] = p.displayName;
                pObj["color"] = p.color;
                pkgsArr.append(pObj);
            }
            root["packages"] = pkgsArr;
            
            QJsonArray notesArr;
            for (const auto &n : cls.notes) {
                QJsonObject nObj;
                nObj["id"] = n.id;
                nObj["text"] = n.text;
                nObj["boundToId"] = n.boundToId;
                nObj["boundToMember"] = n.boundToMember;
                nObj["position"] = n.position;
                nObj["location"] = serializeLocation(n.location);
                notesArr.append(nObj);
            }
            root["notes"] = notesArr;
            
            QJsonArray assocClassesArr;
            for (const auto &ac : cls.associationClasses) {
                QJsonObject acObj;
                acObj["classA"] = ac.classA;
                acObj["classB"] = ac.classB;
                acObj["assocClass"] = ac.assocClass;
                acObj["location"] = serializeLocation(ac.location);
                assocClassesArr.append(acObj);
            }
            root["associationClasses"] = assocClassesArr;
        }
        return root;
    }

    QJsonObject serializeLayout(const RenderDocument &doc) {
        QJsonObject root;
        root["width"] = doc.width;
        root["height"] = doc.height;
        
        QJsonArray nodesArr;
        for (const auto &n : doc.nodes) {
            QJsonObject nObj;
            nObj["id"] = n.id;
            nObj["displayName"] = n.displayName;
            
            QJsonObject rObj;
            rObj["x"] = n.rect.x();
            rObj["y"] = n.rect.y();
            rObj["width"] = n.rect.width();
            rObj["height"] = n.rect.height();
            nObj["rect"] = rObj;
            
            nObj["lifelineLength"] = n.lifelineLength;
            nObj["metaType"] = n.metaType;
            nObj["stereotype"] = n.stereotype;
            nObj["style"] = n.style;
            nObj["location"] = serializeLocation(n.location);
            
            QString kindStr;
            if (n.kind == RenderNodeKind::Participant) kindStr = "Participant";
            else if (n.kind == RenderNodeKind::ClassBox) kindStr = "ClassBox";
            else if (n.kind == RenderNodeKind::Note) kindStr = "Note";
            nObj["kind"] = kindStr;
            
            QJsonArray memsArr;
            for (const auto &m : n.members) {
                memsArr.append(serializeClassMember(m));
            }
            nObj["members"] = memsArr;
            
            nodesArr.append(nObj);
        }
        root["nodes"] = nodesArr;
        
        QJsonArray edgesArr;
        for (const auto &e : doc.edges) {
            QJsonObject eObj;
            eObj["fromNodeId"] = e.fromNodeId;
            eObj["toNodeId"] = e.toNodeId;
            eObj["fromPort"] = e.fromPort;
            eObj["toPort"] = e.toPort;
            eObj["id"] = e.id;
            
            QJsonObject sp;
            sp["x"] = e.startPoint.x();
            sp["y"] = e.startPoint.y();
            eObj["startPoint"] = sp;
            
            QJsonObject ep;
            ep["x"] = e.endPoint.x();
            ep["y"] = e.endPoint.y();
            eObj["endPoint"] = ep;
            
            QJsonArray pts;
            for (const auto &p : e.points) {
                QJsonObject pt;
                pt["x"] = p.x();
                pt["y"] = p.y();
                pts.append(pt);
            }
            eObj["points"] = pts;
            
            eObj["label"] = e.label;
            
            QJsonObject lp;
            lp["x"] = e.labelPosition.x();
            lp["y"] = e.labelPosition.y();
            eObj["labelPosition"] = lp;
            eObj["hasLabelPosition"] = e.hasLabelPosition;
            
            QString kindStr;
            switch (e.kind) {
                case RenderEdgeKind::SyncCall:    kindStr = "SyncCall"; break;
                case RenderEdgeKind::ReplyCall:   kindStr = "ReplyCall"; break;
                case RenderEdgeKind::Inheritance: kindStr = "Inheritance"; break;
                case RenderEdgeKind::Composition: kindStr = "Composition"; break;
                case RenderEdgeKind::Aggregation: kindStr = "Aggregation"; break;
                case RenderEdgeKind::Realization: kindStr = "Realization"; break;
                case RenderEdgeKind::Dependency:  kindStr = "Dependency"; break;
                case RenderEdgeKind::Association: kindStr = "Association"; break;
                case RenderEdgeKind::NoteRelation: kindStr = "NoteRelation"; break;
            }
            eObj["kind"] = kindStr;
            eObj["style"] = e.style;
            eObj["taillabel"] = e.taillabel;
            eObj["headlabel"] = e.headlabel;
            eObj["location"] = serializeLocation(e.location);
            
            edgesArr.append(eObj);
        }
        root["edges"] = edgesArr;
        
        QJsonArray pkgsArr;
        for (const auto &p : doc.packages) {
            QJsonObject pObj;
            pObj["id"] = p.id;
            pObj["displayName"] = p.displayName;
            
            QJsonObject rObj;
            rObj["x"] = p.rect.x();
            rObj["y"] = p.rect.y();
            rObj["width"] = p.rect.width();
            rObj["height"] = p.rect.height();
            pObj["rect"] = rObj;
            
            pObj["color"] = p.color;
            pkgsArr.append(pObj);
        }
        root["packages"] = pkgsArr;
        
        return root;
    }
}

int cli::run(int argc, char *argv[])
{
    QTextStream stdOut(stdout);
    QTextStream stdErr(stderr);

    bool optJsonAst = false;
    bool optJsonLayout = false;
    bool optTrace = false;
    QStringList posArgs;

    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--json-ast") {
            optJsonAst = true;
        } else if (arg == "--json-layout") {
            optJsonLayout = true;
        } else if (arg == "--trace") {
            optTrace = true;
        } else {
            posArgs.append(arg);
        }
    }

    if (posArgs.size() < 3) {
        stdErr << "Usage: plantuml-qt.exe <diagram_type> <file_path> <log_output_path> [--json-ast] [--json-layout] [--trace]\n";
        stdErr << "Example: plantuml-qt.exe class input.puml parse.log --json-ast --json-layout --trace\n";
        stdErr.flush();
        return 1;
    }

    QApplication app(argc, argv);
    QString diagType = posArgs[0].toLower();
    QString filePath = posArgs[1];
    QString logPath = posArgs[2];

    // 1. 读取输入文本文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        stdErr << "Error: Cannot open input file " << filePath << "\n";
        stdErr.flush();
        return 1;
    }
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // 2. 准备输出日志文件
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        stdErr << "Error: Cannot write to log file " << logPath << "\n";
        stdErr.flush();
        return 1;
    }
    QTextStream logOut(&logFile);

    logOut << "========================================\n";
    logOut << "PlantUML CLI Analysis Tool\n";
    logOut << "Input File: " << filePath << "\n";
    logOut << "Assigned Type: " << diagType << "\n";
    logOut << "========================================\n\n";

    stdOut << "Parsing file " << filePath << "...\n";
    stdOut.flush();
    
    // 3. 执行语法解析
    QVector<DiagramBlock> blocks = splitDiagrams(content);
    if (blocks.isEmpty()) {
        DiagramBlock defaultBlock;
        defaultBlock.title = "Diagram 1";
        defaultBlock.content = content;
        defaultBlock.startLine = 1;
        blocks.append(defaultBlock);
    }

    QVector<ParseError> allErrors;
    bool anyParseFailed = false;

    for (const auto &block : blocks) {
        PumlParser parser(block.content);
        ParseResult result = parser.parse();
        if (!result.errors.isEmpty()) {
            anyParseFailed = true;
            for (auto &err : result.errors) {
                err.location.line += block.startLine - 1;
                allErrors.append(err);
            }
        }
    }

    // 4. 输出解析错误
    if (anyParseFailed || !allErrors.isEmpty()) {
        stdErr << "Parse Failed with " << allErrors.size() << " errors:\n";
        logOut << "Parse Failed with " << allErrors.size() << " errors:\n";
        for (const auto &err : allErrors) {
            QString errMsg = QString("  Line %1: %2").arg(err.location.line).arg(err.message);
            stdErr << errMsg << "\n";
            logOut << errMsg << "\n";
        }
        stdErr.flush();
        logFile.close();
        return 1;
    }

    logOut << "Parser successfully created AST for all blocks (0 errors).\n";
    stdOut << "Parser successfully created AST for all blocks (0 errors).\n";
    stdOut.flush();

    // 5. 校验并执行排版生成
    GraphvizLayoutEngine engine;
    for (int i = 0; i < blocks.size(); ++i) {
        const auto &block = blocks[i];
        logOut << "----------------------------------------\n";
        logOut << "Block [" << i << "]: " << block.title << "\n";
        logOut << "Start Line: " << block.startLine << "\n";

        PumlParser parser(block.content);
        ParseResult result = parser.parse();

        if (result.ast) {
            // 如果开启了 --json-ast，将其 AST 导出
            if (optJsonAst) {
                QJsonObject astJson = serializeAst(*result.ast);
                QJsonDocument astDoc(astJson);
                QFileInfo logInfo(logPath);
                QString basePath = logInfo.absolutePath() + "/" + logInfo.completeBaseName();
                QString jsonAstPath = QString("%1_%2_ast.json").arg(basePath).arg(i);
                QFile astFile(jsonAstPath);
                if (astFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    astFile.write(astDoc.toJson(QJsonDocument::Indented));
                    astFile.close();
                }
            }

            bool isSeq = result.ast->isSequence();
            QString detected = isSeq ? "sequence" : "class";
            logOut << "Detected AST Diagram Type: " << detected << "\n";
            
            if (diagType != detected) {
                logOut << "Warning: Assigned type '" << diagType << "' mismatch with detected '" << detected << "'.\n";
            }

            stdOut << "Generating DOT layout for block [" << block.title << "]...\n";
            stdOut.flush();
            RenderDocument doc = engine.layout(*result.ast, RenderTheme());
            
            // 如果开启了 --json-layout，将其 Layout 导出
            if (optJsonLayout) {
                QJsonObject layoutJson = serializeLayout(doc);
                QJsonDocument layoutDoc(layoutJson);
                QFileInfo logInfo(logPath);
                QString basePath = logInfo.absolutePath() + "/" + logInfo.completeBaseName();
                QString jsonLayoutPath = QString("%1_%2_layout.json").arg(basePath).arg(i);
                QFile lFile(jsonLayoutPath);
                if (lFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    lFile.write(layoutDoc.toJson(QJsonDocument::Indented));
                    lFile.close();
                }
            }

            logOut << "Successfully ran Graphviz Layout Engine.\n";
            logOut << "RenderDocument size: " << doc.width << "x" << doc.height << "\n";
            logOut << "Nodes: " << doc.nodes.size() << ", Edges: " << doc.edges.size() << "\n";
            
            stdOut << "Layout run finished. Generated document size: " << doc.width << "x" << doc.height << "\n";
            stdOut.flush();

            // 6. 渲染并保存为 PNG 图片文件
            DiagramScene scene;
            DiagramSceneRenderer renderer;
            renderer.render(&scene, doc, RenderTheme());

            QRectF sceneRect = scene.itemsBoundingRect();
            double padding = 20.0;
            sceneRect.adjust(-padding, -padding, padding, padding);

            if (sceneRect.width() <= 0 || sceneRect.height() <= 0) {
                sceneRect = QRectF(0, 0, 100, 100);
            }

            QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::white);

            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            scene.render(&painter, QRectF(), sceneRect);
            painter.end();

            // 决定导出的图片路径
            QFileInfo logInfo(logPath);
            QString basePath = logInfo.absolutePath() + "/" + logInfo.completeBaseName();
            QString imagePath;
            if (blocks.size() == 1) {
                imagePath = basePath + ".png";
            } else {
                QString safeTitle = block.title;
                safeTitle.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
                imagePath = QString("%1_%2_%3.png").arg(basePath).arg(i).arg(safeTitle);
            }

            if (image.save(imagePath, "PNG")) {
                QString saveMsg = QString("Successfully rendered and saved image to %1").arg(imagePath);
                stdOut << saveMsg << "\n";
                logOut << saveMsg << "\n";
                stdOut.flush();
            } else {
                QString errMsg = QString("Error: Failed to save image to %1").arg(imagePath);
                stdErr << errMsg << "\n";
                logOut << errMsg << "\n";
                stdErr.flush();
            }
        } else {
            logOut << "Error: AST in block [" << block.title << "] is null!\n";
            stdErr << "Error: AST in block [" << block.title << "] is null!\n";
            stdErr.flush();
            logFile.close();
            return 1;
        }
    }

    // 输出行匹配 Tracer 路由日志
    if (optTrace) {
        logOut << "\n========================================\n";
        logOut << "Tracer Route Logs for Blocks:\n";
        logOut << "========================================\n";
        for (int i = 0; i < blocks.size(); ++i) {
            const auto &block = blocks[i];
            PumlParser parser(block.content);
            ParseResult result = parser.parse();
            
            logOut << "\n--- Block [" << i << "]: " << block.title << " (Start Line: " << block.startLine << ") ---\n";
            QRegularExpression lineRx("^Line (\\d+):");
            for (const auto &traceLine : result.traceLog) {
                QString outputTraceLine = traceLine;
                auto match = lineRx.match(traceLine);
                if (match.hasMatch()) {
                    int localLineNum = match.captured(1).toInt();
                    int absLineNum = localLineNum + block.startLine - 1;
                    outputTraceLine.replace(match.captured(0), QString("Line %1:").arg(absLineNum));
                }
                logOut << outputTraceLine << "\n";
            }
        }
    }

    logFile.close();
    return 0;
}

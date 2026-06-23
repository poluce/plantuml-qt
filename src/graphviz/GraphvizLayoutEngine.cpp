#include "GraphvizLayoutEngine.h"
#include <algorithm>
#include <QDebug>
#include <QHash>
#include <QPainterPath>
#include <QSet>
#include <QRegularExpression>

namespace {
    QString sequenceNodeId(const QString &id)
    {
        return "seq_participant_" + id;
    }

    QString sequenceAnchorId(int row, const QString &participantId)
    {
        return QString("seq_anchor_%1_%2").arg(row).arg(participantId);
    }

    QString defaultPackageId()
    {
        return "__default_package__";
    }

    const double sequenceMessageSpacingFallback = 65.0;
    const double classHeaderHeight = 35.0;
    const double classMemberLineHeight = 18.0;

    QString escapeHtml(const QString &text) {
        QString r = text;
        r.replace("&", "&amp;");
        r.replace("<", "&lt;");
        r.replace(">", "&gt;");
        r.replace("\"", "&quot;");
        return r;
    }

    QString buildHtmlLabel(const ClassDecl &klass) {
        // 将 cellpadding 设置为 0，防止默认内边距破坏我们的行高物理对齐
        QString html = "<table border=\"0\" cellborder=\"0\" cellspacing=\"0\" cellpadding=\"0\" bgcolor=\"white\">";
        
        // 头部高度设为 35.0 (Header) + 6.0 (成员上方间隔) = 41 像素
        html += "<tr><td align=\"center\" height=\"41\">";
        if (!klass.stereotype.isEmpty()) {
            html += "&lt;&lt;" + escapeHtml(klass.stereotype) + "&gt;&gt;<br/>";
        }
        html += "<b>" + escapeHtml(klass.displayName.isEmpty() ? klass.id : klass.displayName) + "</b></td></tr>";

        for (int i = 0; i < klass.members.size(); ++i) {
            const auto &member = klass.members[i];
            if (member.isSeparator) {
                // 分割线同样强制限制高度为 18 像素
                if (member.separatorText.isEmpty()) {
                    html += "<tr><td align=\"center\" border=\"1\" sides=\"T\" bgcolor=\"#f3f4f6\" height=\"18\"> </td></tr>";
                } else {
                    html += QString("<tr><td align=\"center\" border=\"1\" sides=\"T\" bgcolor=\"#f3f4f6\" height=\"18\"><font point-size=\"10\"><i>%1</i></font></td></tr>")
                                .arg(escapeHtml(member.separatorText));
                }
            } else {
                // 成员行强制高度为 18 像素，并分配相应端口
                html += QString("<tr><td align=\"left\" port=\"member_%1\" height=\"18\">%2</td></tr>")
                            .arg(QString::number(i), escapeHtml(member.rawText));
            }
        }
        html += "</table>";
        return "<" + html + ">";
    }

    QString formatQualifierAndMultiplicity(const QString &qualifier, const QString &multiplicity) {
        if (qualifier.isEmpty() && multiplicity.isEmpty()) {
            return "";
        }
        if (qualifier.isEmpty()) {
            return multiplicity;
        }
        if (multiplicity.isEmpty()) {
            return QString("[%1]").arg(qualifier);
        }
        return QString("[%1] %2").arg(qualifier, multiplicity);
    }

    QString getClassPackage(const ClassDiagramAst &ast, const QString &classId) {
        for (const auto &klass : ast.classes) {
            if (klass.id == classId) {
                return klass.packageName.isEmpty() ? defaultPackageId() : klass.packageName;
            }
        }
        return defaultPackageId();
    }
}

RenderDocument GraphvizLayoutEngine::layout(const DiagramAst &ast, const RenderTheme &theme)
{
    Q_UNUSED(theme);

    const LayoutGraph layoutGraph = ast.isSequence()
        ? buildSequenceGraph(static_cast<const SequenceDiagramAst&>(ast))
        : buildClassGraph(static_cast<const ClassDiagramAst&>(ast));
    qDebug() << "[Graphviz] LayoutGraph:"
             << "type" << (ast.isSequence() ? "sequence" : "class")
             << "nodes" << layoutGraph.nodes.size()
             << "edges" << layoutGraph.edges.size()
             << "clusters" << layoutGraph.clusters.size()
             << "sameRanks" << layoutGraph.sameRanks.size();

    const QString dot = DotBuilder().build(layoutGraph);
    qDebug() << "\n==== Generated DOT Source ====\n" << dot << "\n==============================\n";
    const DotProcessResult processResult = DotProcessRunner().runJson(dot);
    if (processResult.exitCode != 0 || processResult.stdoutData.isEmpty()) {
        qWarning() << "[Graphviz] 布局失败，返回空 RenderDocument。exitCode:"
                   << processResult.exitCode
                   << "stdoutBytes:" << processResult.stdoutData.size()
                   << "stderr:" << processResult.stderrText;
        RenderDocument empty;
        empty.width = 600.0;
        empty.height = 400.0;
        return empty;
    }

    const GraphLayoutResult layoutResult = DotJsonParser().parse(processResult.stdoutData);
    if (!layoutResult.errorMessage.isEmpty()) {
        qWarning() << "[Graphviz] JSON 结果不可用，返回空 RenderDocument:" << layoutResult.errorMessage;
        RenderDocument empty;
        empty.width = 600.0;
        empty.height = 400.0;
        return empty;
    }

    RenderDocument doc = buildRenderDocument(ast, layoutGraph, layoutResult);
    qDebug() << "[Graphviz] RenderDocument:"
             << "nodes" << doc.nodes.size()
             << "edges" << doc.edges.size()
             << "packages" << doc.packages.size()
             << "size" << doc.width << "x" << doc.height;
    return doc;
}

QSizeF GraphvizLayoutEngine::measureParticipant(const ParticipantDecl &participant) const
{
    const double textWidth = qMax(participant.displayName.length(), participant.id.length()) * 8.0 + 36.0;
    return QSizeF(qMax(120.0, textWidth), 40.0);
}

QSizeF GraphvizLayoutEngine::measureClass(const ClassDecl &klass) const
{
    int maxChars = klass.displayName.isEmpty() ? klass.id.length() : klass.displayName.length();
    for (const auto &member : klass.members) {
        int len = member.isSeparator ? member.separatorText.length() + 8 : member.rawText.length();
        maxChars = qMax(maxChars, len);
    }

    const double width = qMax(160.0, maxChars * 7.5 + 36.0);
    const double height = classHeaderHeight + 12.0 + qMax(1, klass.members.size()) * classMemberLineHeight;
    return QSizeF(width, qMax(70.0, height));
}

LayoutGraph GraphvizLayoutEngine::buildSequenceGraph(const SequenceDiagramAst &ast) const
{
    LayoutGraph graph;
    graph.direction = LayoutGraphDirection::TopToBottom;

    QVector<QString> participantIds;
    for (const auto &participant : ast.participants) {
        participantIds.append(participant.id);

        LayoutNode node;
        node.id = sequenceNodeId(participant.id);
        node.label = participant.displayName;
        node.size = measureParticipant(participant);
        node.renderKind = RenderNodeKind::Participant;
        node.location = participant.location;
        graph.nodes.append(node);
    }
    graph.sameRanks.append({participantIds});
    for (auto &id : graph.sameRanks.last().nodeIds) {
        id = sequenceNodeId(id);
    }

    for (int row = 0; row < ast.statements.size(); ++row) {
        LayoutRank rank;
        for (const auto &participant : ast.participants) {
            LayoutNode anchor;
            anchor.id = sequenceAnchorId(row, participant.id);
            anchor.label = "";
            anchor.size = QSizeF(1.0, 1.0);
            anchor.visible = false;
            anchor.renderKind = RenderNodeKind::Participant;
            graph.nodes.append(anchor);
            rank.nodeIds.append(anchor.id);

            LayoutEdge vertical;
            vertical.id = QString("seq_lifeline_%1_%2").arg(row).arg(participant.id);
            vertical.from = (row == 0) ? sequenceNodeId(participant.id) : sequenceAnchorId(row - 1, participant.id);
            vertical.to = anchor.id;
            vertical.visible = false;
            vertical.constraint = true;
            vertical.attrs["weight"] = "20";
            vertical.attrs["minlen"] = "2";
            graph.edges.append(vertical);
        }
        graph.sameRanks.append(rank);

        for (int i = 1; i < rank.nodeIds.size(); ++i) {
            LayoutEdge orderEdge;
            orderEdge.id = QString("seq_order_%1_%2").arg(row).arg(i);
            orderEdge.from = rank.nodeIds[i - 1];
            orderEdge.to = rank.nodeIds[i];
            orderEdge.visible = false;
            orderEdge.constraint = false;
            orderEdge.attrs["weight"] = "100";
            graph.edges.append(orderEdge);
        }

        const auto &message = ast.statements[row];
        LayoutEdge edge;
        edge.id = QString("seq_message_%1").arg(row);
        edge.from = sequenceAnchorId(row, message.from);
        edge.to = sequenceAnchorId(row, message.to);
        edge.label = message.text;
        edge.renderKind = (message.kind == MessageKind::Sync) ? RenderEdgeKind::SyncCall : RenderEdgeKind::ReplyCall;
        edge.constraint = false;
        edge.location = message.location;
        edge.attrs["dir"] = "none";
        graph.edges.append(edge);
    }

    return graph;
}

LayoutGraph GraphvizLayoutEngine::buildClassGraph(const ClassDiagramAst &ast) const
{
    LayoutGraph graph;
    graph.direction = ast.leftToRight ? LayoutGraphDirection::LeftToRight : LayoutGraphDirection::TopToBottom;

    QHash<QString, int> clusterIndexById;
    for (const auto &pkg : ast.packages) {
        LayoutCluster cluster;
        cluster.id = pkg.id.isEmpty() ? defaultPackageId() : pkg.id;
        cluster.label = pkg.displayName.isEmpty() ? pkg.id : pkg.displayName;
        cluster.color = pkg.color;
        graph.clusters.append(cluster);
        clusterIndexById[cluster.id] = graph.clusters.size() - 1;
    }

    for (const auto &klass : ast.classes) {
        if (ast.removedElements.contains(klass.id) || ast.hiddenElements.contains(klass.id)) {
            continue;
        }
        const QString clusterId = klass.packageName.isEmpty() ? defaultPackageId() : klass.packageName;
        if (!clusterIndexById.contains(clusterId)) {
            LayoutCluster cluster;
            cluster.id = clusterId;
            cluster.label = klass.packageName.isEmpty() ? "Default Layer" : klass.packageName;
            cluster.color = "#f9fafb";
            graph.clusters.append(cluster);
            clusterIndexById[clusterId] = graph.clusters.size() - 1;
        }

        LayoutNode node;
        node.id = klass.id;
        if (!klass.members.isEmpty() || !klass.stereotype.isEmpty()) {
            QString htmlLabel = buildHtmlLabel(klass);
            node.label = htmlLabel;
            node.attrs["label"] = htmlLabel;
        } else {
            node.label = klass.id;
        }
        node.size = measureClass(klass);
        node.clusterId = clusterId;
        node.renderKind = RenderNodeKind::ClassBox;
        node.location = klass.location;

        if (!klass.style.isEmpty()) {
            QStringList parts = klass.style.split(';', Qt::SkipEmptyParts);
            for (const auto &part : parts) {
                QString trimmed = part.trimmed();
                if (trimmed.isEmpty()) continue;
                if (trimmed.startsWith('#')) {
                    node.attrs["fillcolor"] = trimmed;
                    node.attrs["style"] = "filled";
                } else if (trimmed.startsWith("fill=", Qt::CaseInsensitive)) {
                    node.attrs["fillcolor"] = trimmed.mid(5).trimmed();
                    node.attrs["style"] = "filled";
                } else if (trimmed.startsWith("fillcolor=", Qt::CaseInsensitive)) {
                    node.attrs["fillcolor"] = trimmed.mid(10).trimmed();
                    node.attrs["style"] = "filled";
                } else if (trimmed.startsWith("color=", Qt::CaseInsensitive)) {
                    node.attrs["color"] = trimmed.mid(6).trimmed();
                } else if (trimmed.startsWith("fontcolor=", Qt::CaseInsensitive)) {
                    node.attrs["fontcolor"] = trimmed.mid(10).trimmed();
                } else if (trimmed.startsWith("penwidth=", Qt::CaseInsensitive)) {
                    node.attrs["penwidth"] = trimmed.mid(9).trimmed();
                } else if (trimmed.startsWith("line:", Qt::CaseInsensitive)) {
                    node.attrs["color"] = trimmed.mid(5).trimmed();
                } else if (trimmed.startsWith("border:", Qt::CaseInsensitive)) {
                    node.attrs["color"] = trimmed.mid(7).trimmed();
                } else if (trimmed.startsWith("line.dashed", Qt::CaseInsensitive) || trimmed.startsWith("border.dashed", Qt::CaseInsensitive)) {
                    QString currentStyle = node.attrs.value("style", "");
                    if (currentStyle.isEmpty()) {
                        node.attrs["style"] = "dashed";
                    } else if (!currentStyle.contains("dashed")) {
                        node.attrs["style"] = currentStyle + ",dashed";
                    }
                } else if (trimmed.startsWith("line.dotted", Qt::CaseInsensitive) || trimmed.startsWith("border.dotted", Qt::CaseInsensitive)) {
                    QString currentStyle = node.attrs.value("style", "");
                    if (currentStyle.isEmpty()) {
                        node.attrs["style"] = "dotted";
                    } else if (!currentStyle.contains("dotted")) {
                        node.attrs["style"] = currentStyle + ",dotted";
                    }
                } else if (trimmed.startsWith("line.bold", Qt::CaseInsensitive) || trimmed.startsWith("border.bold", Qt::CaseInsensitive)) {
                    node.attrs["penwidth"] = "2.5";
                } else if (trimmed == "dashed" || trimmed == "dotted") {
                    QString currentStyle = node.attrs.value("style", "");
                    if (currentStyle.isEmpty()) {
                        node.attrs["style"] = trimmed;
                    } else if (!currentStyle.contains(trimmed)) {
                        node.attrs["style"] = currentStyle + "," + trimmed;
                    }
                }
            }
        }

        graph.nodes.append(node);
        graph.clusters[clusterIndexById[clusterId]].nodeIds.append(node.id);
    }

    struct AssocInfo {
        QString classA;
        QString classB;
        QString assocClass;
        bool processed = false;
        SourceLocation location;
    };
    QVector<AssocInfo> assocInfos;
    for (const auto &ac : ast.associationClasses) {
        assocInfos.append({ac.classA, ac.classB, ac.assocClass, false, ac.location});
    }

    for (int i = 0; i < ast.relations.size(); ++i) {
        const auto &relation = ast.relations[i];
        if (ast.removedElements.contains(relation.from) || ast.hiddenElements.contains(relation.from) ||
            ast.removedElements.contains(relation.to) || ast.hiddenElements.contains(relation.to)) {
            continue;
        }

        // 成员端口匹配 Lambda 函数
        auto matchMemberPort = [&](const QString &classId, const QString &memberName) -> QString {
            if (memberName.isEmpty()) return "";
            QString cleanMember = memberName;
            if (cleanMember.startsWith('"') && cleanMember.endsWith('"') && cleanMember.length() >= 2) {
                cleanMember = cleanMember.mid(1, cleanMember.length() - 2);
            }
            for (const auto &klass : ast.classes) {
                if (klass.id == classId) {
                    for (int k = 0; k < klass.members.size(); ++k) {
                        if (!klass.members[k].isSeparator && klass.members[k].rawText.contains(cleanMember)) {
                            return QString("member_%1").arg(k);
                        }
                    }
                    break;
                }
            }
            return "";
        };

        // 统一计算多重度+限定词
        QString tailVal, headVal;
        QString edgeFrom, edgeTo;
        QString edgeFromPort, edgeToPort;
        if (relation.direction == "left" || relation.direction == "up") {
            edgeFrom = relation.to;
            edgeTo = relation.from;
            edgeFromPort = matchMemberPort(relation.to, relation.toMember);
            edgeToPort = matchMemberPort(relation.from, relation.fromMember);
            tailVal = formatQualifierAndMultiplicity(relation.toQualifier, relation.toMultiplicity);
            headVal = formatQualifierAndMultiplicity(relation.fromQualifier, relation.fromMultiplicity);
        } else {
            edgeFrom = relation.from;
            edgeTo = relation.to;
            edgeFromPort = matchMemberPort(relation.from, relation.fromMember);
            edgeToPort = matchMemberPort(relation.to, relation.toMember);
            tailVal = formatQualifierAndMultiplicity(relation.fromQualifier, relation.fromMultiplicity);
            headVal = formatQualifierAndMultiplicity(relation.toQualifier, relation.toMultiplicity);
        }

        int matchedAssocIdx = -1;
        for (int j = 0; j < assocInfos.size(); ++j) {
            const auto &info = assocInfos[j];
            if ((relation.from == info.classA && relation.to == info.classB) ||
                (relation.from == info.classB && relation.to == info.classA)) {
                matchedAssocIdx = j;
                break;
            }
        }

        if (matchedAssocIdx != -1) {
            assocInfos[matchedAssocIdx].processed = true;

            QString assocPointId = QString("assoc_point_%1").arg(matchedAssocIdx);
            QString assocPointCluster = getClassPackage(ast, relation.from);

            bool alreadyExists = false;
            for (const auto &n : graph.nodes) {
                if (n.id == assocPointId) {
                    alreadyExists = true;
                    break;
                }
            }
            if (!alreadyExists) {
                LayoutNode assocPoint;
                assocPoint.id = assocPointId;
                assocPoint.label = "";
                assocPoint.size = QSizeF(1.0, 1.0);
                assocPoint.renderKind = RenderNodeKind::ClassBox;
                assocPoint.location = relation.location;
                assocPoint.attrs["shape"] = "point";
                assocPoint.attrs["width"] = "0";
                assocPoint.attrs["height"] = "0";
                assocPoint.clusterId = assocPointCluster;
                graph.nodes.append(assocPoint);
                if (clusterIndexById.contains(assocPointCluster)) {
                    graph.clusters[clusterIndexById[assocPointCluster]].nodeIds.append(assocPointId);
                }
            }

            LayoutEdge edgeA;
            edgeA.id = QString("class_relation_%1_split_a").arg(i);
            edgeA.from = edgeFrom;
            edgeA.to = assocPointId;
            edgeA.fromPort = edgeFromPort;
            edgeA.label = "";
            edgeA.renderKind = relationKindToRenderKind(relation.kind);
            edgeA.location = relation.location;
            edgeA.constraint = true;
            edgeA.style = relation.style;
            edgeA.taillabel = tailVal;
            edgeA.headlabel = "";
            edgeA.attrs["arrowhead"] = "none";
            edgeA.attrs["dir"] = "none";
            if (!tailVal.isEmpty()) {
                edgeA.attrs["taillabel"] = tailVal;
            }

            LayoutEdge edgeB;
            edgeB.id = QString("class_relation_%1_split_b").arg(i);
            edgeB.from = assocPointId;
            edgeB.to = edgeTo;
            edgeB.toPort = edgeToPort;
            edgeB.label = relation.text;
            edgeB.renderKind = relationKindToRenderKind(relation.kind);
            edgeB.location = relation.location;
            edgeB.constraint = true;
            edgeB.style = relation.style;
            edgeB.taillabel = "";
            edgeB.headlabel = headVal;
            edgeB.attrs["dir"] = "none";
            if (!headVal.isEmpty()) {
                edgeB.attrs["headlabel"] = headVal;
            }

            if (!tailVal.isEmpty() || !headVal.isEmpty()) {
                edgeA.attrs["labelfontsize"] = "9";
                edgeB.attrs["labelfontsize"] = "9";
            }

            auto applyStyle = [](LayoutEdge &e, const QString &styleStr) {
                QStringList parts = styleStr.split(QRegularExpression("[,;]"), Qt::SkipEmptyParts);
                for (const auto &part : parts) {
                    QString trimmed = part.trimmed();
                    if (trimmed.isEmpty()) continue;
                    if (trimmed.startsWith("line:", Qt::CaseInsensitive)) {
                        e.attrs["color"] = trimmed.mid(5).trimmed();
                    } else if (trimmed.startsWith("text:", Qt::CaseInsensitive)) {
                        e.attrs["fontcolor"] = trimmed.mid(5).trimmed();
                    } else if (trimmed.startsWith("line.dashed", Qt::CaseInsensitive)) {
                        e.attrs["style"] = "dashed";
                    } else if (trimmed.startsWith("line.dotted", Qt::CaseInsensitive)) {
                        e.attrs["style"] = "dotted";
                    } else if (trimmed.startsWith("line.bold", Qt::CaseInsensitive)) {
                        e.attrs["penwidth"] = "2.5";
                    } else if (trimmed.startsWith("thickness=", Qt::CaseInsensitive)) {
                        e.attrs["penwidth"] = trimmed.mid(10).trimmed();
                    } else if (trimmed.startsWith("penwidth=", Qt::CaseInsensitive)) {
                        e.attrs["penwidth"] = trimmed.mid(9).trimmed();
                    } else if (trimmed.startsWith("color=", Qt::CaseInsensitive)) {
                        e.attrs["color"] = trimmed.mid(6).trimmed();
                    } else if (trimmed.startsWith("fontcolor=", Qt::CaseInsensitive)) {
                        e.attrs["fontcolor"] = trimmed.mid(10).trimmed();
                    } else if (trimmed == "dashed" || trimmed == "dotted") {
                        e.attrs["style"] = trimmed;
                    } else if (trimmed.startsWith('#')) {
                        if (trimmed.startsWith("#line:", Qt::CaseInsensitive)) {
                            e.attrs["color"] = trimmed.mid(6).trimmed();
                        } else if (trimmed.startsWith("#text:", Qt::CaseInsensitive)) {
                            e.attrs["fontcolor"] = trimmed.mid(6).trimmed();
                        } else if (trimmed.startsWith("#line.dashed", Qt::CaseInsensitive)) {
                            e.attrs["style"] = "dashed";
                        } else if (trimmed.startsWith("#line.dotted", Qt::CaseInsensitive)) {
                            e.attrs["style"] = "dotted";
                        } else if (trimmed.startsWith("#line.bold", Qt::CaseInsensitive)) {
                            e.attrs["penwidth"] = "2.5";
                        } else {
                            e.attrs["color"] = trimmed;
                        }
                    }
                }
            };

            if (!relation.style.isEmpty()) {
                applyStyle(edgeA, relation.style);
                applyStyle(edgeB, relation.style);
            }
            // 强行把 edgeA arrowhead 设为 none
            edgeA.attrs["arrowhead"] = "none";

            graph.edges.append(edgeA);
            graph.edges.append(edgeB);

            LayoutEdge edgeAssoc;
            edgeAssoc.id = QString("assoc_relation_%1_link").arg(matchedAssocIdx);
            edgeAssoc.from = assocPointId;
            edgeAssoc.to = assocInfos[matchedAssocIdx].assocClass;
            edgeAssoc.label = "";
            edgeAssoc.renderKind = RenderEdgeKind::Dependency;
            edgeAssoc.location = assocInfos[matchedAssocIdx].location;
            edgeAssoc.constraint = false;
            edgeAssoc.visible = true;
            edgeAssoc.attrs["style"] = "dashed";
            edgeAssoc.attrs["arrowhead"] = "none";
            edgeAssoc.attrs["dir"] = "none";
            graph.edges.append(edgeAssoc);

        } else {
            LayoutEdge edge;
            edge.id = QString("class_relation_%1").arg(i);
            edge.from = edgeFrom;
            edge.to = edgeTo;
            edge.fromPort = edgeFromPort;
            edge.toPort = edgeToPort;
            edge.style = relation.style;
            edge.taillabel = tailVal;
            edge.headlabel = headVal;
            if (!tailVal.isEmpty()) {
                edge.attrs["taillabel"] = tailVal;
            }
            if (!headVal.isEmpty()) {
                edge.attrs["headlabel"] = headVal;
            }
            if (!tailVal.isEmpty() || !headVal.isEmpty()) {
                edge.attrs["labelfontsize"] = "9";
            }
            edge.label = relation.text;
            edge.renderKind = relationKindToRenderKind(relation.kind);
            edge.location = relation.location;
            edge.attrs["dir"] = "none";

            if (!relation.style.isEmpty()) {
                QStringList parts = relation.style.split(QRegularExpression("[,;]"), Qt::SkipEmptyParts);
                for (const auto &part : parts) {
                    QString trimmed = part.trimmed();
                    if (trimmed.isEmpty()) continue;
                    if (trimmed.startsWith("line:", Qt::CaseInsensitive)) {
                        edge.attrs["color"] = trimmed.mid(5).trimmed();
                    } else if (trimmed.startsWith("text:", Qt::CaseInsensitive)) {
                        edge.attrs["fontcolor"] = trimmed.mid(5).trimmed();
                    } else if (trimmed.startsWith("line.dashed", Qt::CaseInsensitive)) {
                        edge.attrs["style"] = "dashed";
                    } else if (trimmed.startsWith("line.dotted", Qt::CaseInsensitive)) {
                        edge.attrs["style"] = "dotted";
                    } else if (trimmed.startsWith("line.bold", Qt::CaseInsensitive)) {
                        edge.attrs["penwidth"] = "2.5";
                    } else if (trimmed.startsWith("thickness=", Qt::CaseInsensitive)) {
                        edge.attrs["penwidth"] = trimmed.mid(10).trimmed();
                    } else if (trimmed.startsWith("penwidth=", Qt::CaseInsensitive)) {
                        edge.attrs["penwidth"] = trimmed.mid(9).trimmed();
                    } else if (trimmed.startsWith("color=", Qt::CaseInsensitive)) {
                        edge.attrs["color"] = trimmed.mid(6).trimmed();
                    } else if (trimmed.startsWith("fontcolor=", Qt::CaseInsensitive)) {
                        edge.attrs["fontcolor"] = trimmed.mid(10).trimmed();
                    } else if (trimmed == "dashed" || trimmed == "dotted") {
                        edge.attrs["style"] = trimmed;
                    } else if (trimmed.startsWith('#')) {
                        if (trimmed.startsWith("#line:", Qt::CaseInsensitive)) {
                            edge.attrs["color"] = trimmed.mid(6).trimmed();
                        } else if (trimmed.startsWith("#text:", Qt::CaseInsensitive)) {
                            edge.attrs["fontcolor"] = trimmed.mid(6).trimmed();
                        } else if (trimmed.startsWith("#line.dashed", Qt::CaseInsensitive)) {
                            edge.attrs["style"] = "dashed";
                        } else if (trimmed.startsWith("#line.dotted", Qt::CaseInsensitive)) {
                            edge.attrs["style"] = "dotted";
                        } else if (trimmed.startsWith("#line.bold", Qt::CaseInsensitive)) {
                            edge.attrs["penwidth"] = "2.5";
                        } else {
                            edge.attrs["color"] = trimmed;
                        }
                    }
                }
            }
            graph.edges.append(edge);
        }
    }

    // 遍历 ast.notes，将 Note 转化为 LayoutNode 并存入 nodes，同时处理与被绑定类的连线
    for (int i = 0; i < ast.notes.size(); ++i) {
        const auto &note = ast.notes[i];
        LayoutNode node;
        node.id = note.id.isEmpty() ? ("note_" + QString::number(i)) : note.id;
        node.label = note.text;

        // 计算宽高
        double maxLogicalLen = 0.0;
        QStringList lines = note.text.split('\n');
        for (const auto &l : lines) {
            double currentLen = 0.0;
            for (const QChar &ch : l) {
                currentLen += (ch.unicode() > 127) ? 1.8 : 1.0;
            }
            if (currentLen > maxLogicalLen) {
                maxLogicalLen = currentLen;
            }
        }
        double width = qMax(120.0, maxLogicalLen * 8.0 + 24.0);
        double height = qMax(40.0, lines.size() * 18.0 + 20.0);
        node.size = QSizeF(width, height);

        QString clusterId = defaultPackageId();
        if (!note.boundToId.isEmpty()) {
            for (const auto &n : graph.nodes) {
                if (n.id == note.boundToId) {
                    clusterId = n.clusterId;
                    break;
                }
            }
        }

        if (!clusterIndexById.contains(clusterId)) {
            LayoutCluster cluster;
            cluster.id = clusterId;
            cluster.label = (clusterId == defaultPackageId()) ? "Default Layer" : clusterId;
            cluster.color = "#f9fafb";
            graph.clusters.append(cluster);
            clusterIndexById[clusterId] = graph.clusters.size() - 1;
        }

        node.clusterId = clusterId;
        node.renderKind = RenderNodeKind::Note;
        node.location = note.location;

        graph.nodes.append(node);
        graph.clusters[clusterIndexById[clusterId]].nodeIds.append(node.id);

        if (!note.boundToId.isEmpty() &&
            !ast.removedElements.contains(note.boundToId) &&
            !ast.hiddenElements.contains(note.boundToId)) {
            LayoutEdge noteEdge;
            noteEdge.id = QString("note_relation_%1").arg(i);
            noteEdge.from = node.id;
            noteEdge.to = note.boundToId;
            noteEdge.label = "";
            noteEdge.renderKind = RenderEdgeKind::NoteRelation;
            noteEdge.location = note.location;
            noteEdge.constraint = false;
            noteEdge.visible = true;
            noteEdge.attrs["style"] = "dashed";
            noteEdge.attrs["arrowhead"] = "none";
            noteEdge.attrs["dir"] = "none";

            // 如果绑定了成员，则尝试查找类中对应的成员位置，分配端口并使能布局约束
            if (!note.boundToMember.isEmpty()) {
                QString cleanMember = note.boundToMember;
                if (cleanMember.startsWith('"') && cleanMember.endsWith('"') && cleanMember.length() >= 2) {
                    cleanMember = cleanMember.mid(1, cleanMember.length() - 2);
                }

                int targetMemberIdx = -1;
                for (const auto &klass : ast.classes) {
                    if (klass.id == note.boundToId) {
                        for (int k = 0; k < klass.members.size(); ++k) {
                            if (!klass.members[k].isSeparator && klass.members[k].rawText.contains(cleanMember)) {
                                targetMemberIdx = k;
                                break;
                            }
                        }
                        break;
                    }
                }

                if (targetMemberIdx != -1) {
                    noteEdge.toPort = QString("member_%1").arg(targetMemberIdx);
                    noteEdge.constraint = true;
                }
            }

            graph.edges.append(noteEdge);
        }
    }

    // 遍历未处理的关系类并自动隐式创建拆分边连线
    for (int k = 0; k < assocInfos.size(); ++k) {
        if (assocInfos[k].processed) {
            continue;
        }
        if (ast.removedElements.contains(assocInfos[k].classA) || ast.hiddenElements.contains(assocInfos[k].classA) ||
            ast.removedElements.contains(assocInfos[k].classB) || ast.hiddenElements.contains(assocInfos[k].classB) ||
            ast.removedElements.contains(assocInfos[k].assocClass) || ast.hiddenElements.contains(assocInfos[k].assocClass)) {
            continue;
        }

        QString assocPointId = QString("assoc_point_implicit_%1").arg(k);
        QString assocPointCluster = getClassPackage(ast, assocInfos[k].classA);

        LayoutNode assocPoint;
        assocPoint.id = assocPointId;
        assocPoint.label = "";
        assocPoint.size = QSizeF(1.0, 1.0);
        assocPoint.renderKind = RenderNodeKind::ClassBox;
        assocPoint.location = assocInfos[k].location;
        assocPoint.attrs["shape"] = "point";
        assocPoint.attrs["width"] = "0";
        assocPoint.attrs["height"] = "0";
        assocPoint.clusterId = assocPointCluster;
        graph.nodes.append(assocPoint);
        if (clusterIndexById.contains(assocPointCluster)) {
            graph.clusters[clusterIndexById[assocPointCluster]].nodeIds.append(assocPointId);
        }

        LayoutEdge edgeA;
        edgeA.id = QString("class_relation_implicit_%1_split_a").arg(k);
        edgeA.from = assocInfos[k].classA;
        edgeA.to = assocPointId;
        edgeA.label = "";
        edgeA.renderKind = RenderEdgeKind::Association;
        edgeA.location = assocInfos[k].location;
        edgeA.constraint = true;
        edgeA.attrs["arrowhead"] = "none";
        edgeA.attrs["dir"] = "none";
        graph.edges.append(edgeA);

        LayoutEdge edgeB;
        edgeB.id = QString("class_relation_implicit_%1_split_b").arg(k);
        edgeB.from = assocPointId;
        edgeB.to = assocInfos[k].classB;
        edgeB.label = "";
        edgeB.renderKind = RenderEdgeKind::Association;
        edgeB.location = assocInfos[k].location;
        edgeB.constraint = true;
        edgeB.attrs["dir"] = "none";
        graph.edges.append(edgeB);

        LayoutEdge edgeAssoc;
        edgeAssoc.id = QString("assoc_relation_implicit_%1_link").arg(k);
        edgeAssoc.from = assocPointId;
        edgeAssoc.to = assocInfos[k].assocClass;
        edgeAssoc.label = "";
        edgeAssoc.renderKind = RenderEdgeKind::Dependency;
        edgeAssoc.location = assocInfos[k].location;
        edgeAssoc.constraint = false;
        edgeAssoc.visible = true;
        edgeAssoc.attrs["style"] = "dashed";
        edgeAssoc.attrs["arrowhead"] = "none";
        edgeAssoc.attrs["dir"] = "none";
        graph.edges.append(edgeAssoc);
    }

    return graph;
}

RenderEdgeKind GraphvizLayoutEngine::relationKindToRenderKind(RelationKind kind) const
{
    switch (kind) {
    case RelationKind::Inheritance: return RenderEdgeKind::Inheritance;
    case RelationKind::Composition: return RenderEdgeKind::Composition;
    case RelationKind::Aggregation: return RenderEdgeKind::Aggregation;
    case RelationKind::Realization: return RenderEdgeKind::Realization;
    case RelationKind::Dependency: return RenderEdgeKind::Dependency;
    case RelationKind::Association: return RenderEdgeKind::Association;
    }
    return RenderEdgeKind::Association;
}

QPainterPath GraphvizLayoutEngine::pathFromPoints(const QVector<QPointF> &points) const
{
    QPainterPath path;
    if (points.isEmpty()) {
        return path;
    }
    path.moveTo(points.first());
    for (int i = 1; i < points.size(); ++i) {
        path.lineTo(points[i]);
    }
    return path;
}

RenderDocument GraphvizLayoutEngine::buildRenderDocument(
    const DiagramAst &ast,
    const LayoutGraph &layoutGraph,
    const GraphLayoutResult &layoutResult) const
{
    RenderDocument doc;
    doc.width = qMax(600.0, layoutResult.graphBounds.width() + 80.0);
    doc.height = qMax(400.0, layoutResult.graphBounds.height() + 80.0);

    QHash<QString, LayoutNode> layoutNodes;
    for (const auto &node : layoutGraph.nodes) {
        layoutNodes[node.id] = node;
    }
    QHash<QString, LayoutEdge> layoutEdges;
    for (const auto &edge : layoutGraph.edges) {
        layoutEdges[edge.id] = edge;
    }

    const QPointF offset(40.0, 40.0);

    if (ast.isSequence()) {
        const auto &seq = static_cast<const SequenceDiagramAst&>(ast);
        for (const auto &participant : seq.participants) {
            const QString nodeId = sequenceNodeId(participant.id);
            if (!layoutResult.nodesById.contains(nodeId)) {
                continue;
            }
            RenderNode node;
            node.id = participant.id;
            node.displayName = participant.displayName;
            node.rect = layoutResult.nodesById[nodeId].rect.translated(offset);
            node.kind = RenderNodeKind::Participant;
            node.location = participant.location;

            double lastY = node.rect.bottom() + 120.0;
            for (int row = 0; row < seq.statements.size(); ++row) {
                const QString anchorId = sequenceAnchorId(row, participant.id);
                if (layoutResult.nodesById.contains(anchorId)) {
                    lastY = qMax(lastY, layoutResult.nodesById[anchorId].rect.center().y() + offset.y());
                }
            }
            node.lifelineLength = qMax(120.0, lastY - node.rect.bottom() + 20.0);
            doc.nodes.append(node);
        }
    } else {
        const auto &classAst = static_cast<const ClassDiagramAst&>(ast);
        QHash<QString, ClassDecl> classById;
        for (const auto &klass : classAst.classes) {
            classById[klass.id] = klass;
        }

        for (const auto &cluster : layoutGraph.clusters) {
            if (!layoutResult.clustersById.contains(cluster.id)) {
                continue;
            }
            RenderPackage pkg;
            pkg.id = cluster.id == defaultPackageId() ? "" : cluster.id;
            pkg.displayName = cluster.label;
            pkg.color = cluster.color;
            pkg.rect = layoutResult.clustersById[cluster.id].rect.translated(offset);
            doc.packages.append(pkg);
        }

        for (const auto &klass : classAst.classes) {
            if (!layoutResult.nodesById.contains(klass.id)) {
                continue;
            }
            RenderNode node;
            node.id = klass.id;
            node.displayName = klass.displayName.isEmpty() ? klass.id : klass.displayName;
            node.rect = layoutResult.nodesById[klass.id].rect.translated(offset);
            node.members = klass.members;
            node.metaType = klass.metaType;
            node.kind = RenderNodeKind::ClassBox;
            node.location = klass.location;
            node.stereotype = klass.stereotype;
            node.style = klass.style;
            doc.nodes.append(node);
        }

        // 装载 Note 节点
        for (const auto &layoutNode : layoutGraph.nodes) {
            if (layoutNode.renderKind == RenderNodeKind::Note) {
                if (!layoutResult.nodesById.contains(layoutNode.id)) {
                    continue;
                }
                RenderNode node;
                node.id = layoutNode.id;
                node.displayName = layoutNode.label;
                node.rect = layoutResult.nodesById[layoutNode.id].rect.translated(offset);
                node.kind = RenderNodeKind::Note;
                node.location = layoutNode.location;
                doc.nodes.append(node);
            }
        }
    }

    for (const auto &layoutEdge : layoutGraph.edges) {
        if (!layoutEdge.visible) {
            continue;
        }
        const auto it = std::find_if(layoutResult.edges.constBegin(), layoutResult.edges.constEnd(), [&](const GraphLayoutEdge &edge) {
            return edge.id == layoutEdge.id;
        });
        if (it == layoutResult.edges.constEnd()) {
            continue;
        }

        RenderEdge edge;
        edge.id = layoutEdge.id;
        edge.fromNodeId = layoutEdge.from;
        edge.toNodeId = layoutEdge.to;
        edge.fromPort = layoutEdge.fromPort;
        edge.toPort = layoutEdge.toPort;
        edge.label = layoutEdge.label;
        edge.kind = layoutEdge.renderKind;
        edge.location = layoutEdge.location;
        edge.style = layoutEdge.style;
        edge.taillabel = layoutEdge.taillabel;
        edge.headlabel = layoutEdge.headlabel;
        for (const auto &point : it->points) {
            edge.points.append(point + offset);
        }
        if (!edge.points.isEmpty()) {
            // 在后端高精度物理校准连线的起点和终点坐标，使其首帧即完美吸附
            edge.points[0] = calculatePortPoint(edge.fromNodeId, edge.fromPort, layoutResult.nodesById, ast, offset, edge.points.first(), edge.toNodeId);
            edge.points[edge.points.size() - 1] = calculatePortPoint(edge.toNodeId, edge.toPort, layoutResult.nodesById, ast, offset, edge.points.last(), edge.fromNodeId);

            edge.startPoint = edge.points.first();
            edge.endPoint = edge.points.last();
            edge.path = pathFromPoints(edge.points);
        }
        if (it->hasLabelPosition) {
            edge.labelPosition = it->labelPosition + offset;
            edge.hasLabelPosition = true;
        }
        doc.edges.append(edge);
    }

    if (ast.isSequence()) {
        const auto &seq = static_cast<const SequenceDiagramAst&>(ast);
        for (auto &edge : doc.edges) {
            if (!edge.id.startsWith("seq_message_")) {
                continue;
            }
            const int row = edge.id.mid(QString("seq_message_").length()).toInt();
            if (row >= 0 && row < seq.statements.size()) {
                edge.fromNodeId = seq.statements[row].from;
                edge.toNodeId = seq.statements[row].to;
            }
        }
    }

    return doc;
}

const ClassDecl *GraphvizLayoutEngine::findClassDecl(const DiagramAst &ast, const QString &classId) const
{
    if (ast.isSequence()) {
        return nullptr;
    }
    const auto &classAst = static_cast<const ClassDiagramAst&>(ast);
    for (const auto &klass : classAst.classes) {
        if (klass.id == classId) {
            return &klass;
        }
    }
    return nullptr;
}

QPointF GraphvizLayoutEngine::calculatePortPoint(
    const QString &nodeId,
    const QString &portId,
    const QHash<QString, GraphLayoutNode> &nodesById,
    const DiagramAst &ast,
    const QPointF &offset,
    const QPointF &fallback,
    const QString &otherNodeId) const
{
    if (!nodesById.contains(nodeId)) {
        return fallback;
    }
    QRectF nodeRect = nodesById[nodeId].rect.translated(offset);

    // 1. 判断是否是 Note 节点
    bool isNote = nodeId.startsWith("note_");
    if (!isNote && !ast.isSequence()) {
        const auto &classAst = static_cast<const ClassDiagramAst&>(ast);
        for (const auto &note : classAst.notes) {
            if (note.id == nodeId || (note.id.isEmpty() && nodeId.startsWith("note_"))) {
                isNote = true;
                break;
            }
        }
    }

    // 2. 如果是 Note 节点：Y 轴对齐 Note 侧边中心，X 轴物理贴边
    if (isNote) {
        double targetY = nodeRect.center().y();
        double targetX = fallback.x();
        if (nodesById.contains(otherNodeId)) {
            QRectF otherRect = nodesById[otherNodeId].rect.translated(offset);
            targetX = (otherRect.center().x() > nodeRect.center().x()) ? nodeRect.right() : nodeRect.left();
        }
        return QPointF(targetX, targetY);
    }

    // 3. 如果是普通 Class 连线（没有指定 member_ 端口）：保持 Graphviz 的 Y 轴排布，强制规范 X 轴贴边
    if (portId.isEmpty() || !portId.startsWith("member_")) {
        double targetX = fallback.x();
        if (nodesById.contains(otherNodeId)) {
            QRectF otherRect = nodesById[otherNodeId].rect.translated(offset);
            targetX = (otherRect.center().x() > nodeRect.center().x()) ? nodeRect.right() : nodeRect.left();
        }
        return QPointF(targetX, fallback.y());
    }

    // 4. 如果是 Class 且指定了特定的成员端口：Y 轴对准成员文字中线，X 轴物理贴边
    const ClassDecl *klass = findClassDecl(ast, nodeId);
    if (!klass) {
        return fallback;
    }

    bool ok = false;
    int memberIdx = portId.mid(7).toInt(&ok);
    if (!ok || memberIdx < 0 || memberIdx >= klass->members.size()) {
        return fallback;
    }

    double localY = classHeaderHeight + 6.0 + memberIdx * classMemberLineHeight + classMemberLineHeight / 2.0;
    double targetY = nodeRect.top() + localY;

    double targetX = fallback.x();
    if (nodesById.contains(otherNodeId)) {
        QRectF otherRect = nodesById[otherNodeId].rect.translated(offset);
        targetX = (otherRect.center().x() > nodeRect.center().x()) ? nodeRect.right() : nodeRect.left();
    }

    return QPointF(targetX, targetY);
}

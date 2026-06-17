#include "GraphvizLayoutEngine.h"
#include <algorithm>
#include <QDebug>
#include <QHash>
#include <QPainterPath>
#include <QSet>

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
    int maxChars = klass.id.length();
    for (const auto &member : klass.members) {
        maxChars = qMax(maxChars, member.length());
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
        node.label = klass.id;
        node.size = measureClass(klass);
        node.clusterId = clusterId;
        node.renderKind = RenderNodeKind::ClassBox;
        node.location = klass.location;
        graph.nodes.append(node);
        graph.clusters[clusterIndexById[clusterId]].nodeIds.append(node.id);
    }

    for (int i = 0; i < ast.relations.size(); ++i) {
        const auto &relation = ast.relations[i];
        LayoutEdge edge;
        edge.id = QString("class_relation_%1").arg(i);
        edge.from = relation.from;
        edge.to = relation.to;
        edge.label = relation.text;
        edge.renderKind = relationKindToRenderKind(relation.kind);
        edge.location = relation.location;
        edge.attrs["dir"] = "none";
        graph.edges.append(edge);

        if (relation.direction == "left" || relation.direction == "up") {
            LayoutEdge constraint;
            constraint.id = QString("class_relation_constraint_%1").arg(i);
            constraint.from = relation.to;
            constraint.to = relation.from;
            constraint.visible = false;
            constraint.constraint = true;
            constraint.attrs["weight"] = "8";
            graph.edges.append(constraint);
        }
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
            node.displayName = klass.id;
            node.rect = layoutResult.nodesById[klass.id].rect.translated(offset);
            node.members = klass.members;
            node.metaType = klass.metaType;
            node.kind = RenderNodeKind::ClassBox;
            node.location = klass.location;
            doc.nodes.append(node);
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
        edge.label = layoutEdge.label;
        edge.kind = layoutEdge.renderKind;
        edge.location = layoutEdge.location;
        for (const auto &point : it->points) {
            edge.points.append(point + offset);
        }
        if (!edge.points.isEmpty()) {
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

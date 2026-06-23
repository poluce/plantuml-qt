#include "DotBuilder.h"
#include <QSet>
#include <QStringList>

QString DotBuilder::quote(const QString &value) const
{
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    return "\"" + escaped + "\"";
}

QString DotBuilder::attrs(const QHash<QString, QString> &values) const
{
    QStringList parts;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (it.key() == "label" && it.value().startsWith("<") && it.value().endsWith(">")) {
            parts << QString("%1=%2").arg(it.key(), it.value());
        } else {
            parts << QString("%1=%2").arg(it.key(), quote(it.value()));
        }
    }
    return parts.isEmpty() ? QString() : " [" + parts.join(", ") + "]";
}

QString DotBuilder::build(const LayoutGraph &graph) const
{
    QString dot;
    dot += "digraph G {\n";
    dot += "  graph [compound=true, splines=ortho, outputorder=edgesfirst, nodesep=\"0.7\", ranksep=\"0.85\", rankdir=\"";
    dot += (graph.direction == LayoutGraphDirection::LeftToRight) ? "LR" : "TB";
    dot += "\"];\n";
    dot += "  node [shape=box, margin=0, fixedsize=true, fontname=\"Arial\"];\n";
    dot += "  edge [fontname=\"Arial\"];\n";

    QSet<QString> clusteredNodes;
    for (const auto &cluster : graph.clusters) {
        dot += QString("  subgraph %1 {\n").arg(quote("cluster_" + cluster.id));
        if (!cluster.color.isEmpty()) {
            dot += QString("    graph [id=%1, label=%2, margin=\"18\", color=\"#cccccc\", bgcolor=%3];\n")
                       .arg(quote(cluster.id), quote(cluster.label), quote(cluster.color));
        } else {
            dot += QString("    graph [id=%1, label=%2, margin=\"18\", color=\"transparent\"];\n")
                       .arg(quote(cluster.id), quote(cluster.label));
        }
        for (const auto &nodeId : cluster.nodeIds) {
            for (const auto &node : graph.nodes) {
                if (node.id != nodeId) {
                    continue;
                }
                QHash<QString, QString> nodeAttrs = node.attrs;
                nodeAttrs["id"] = node.id;
                nodeAttrs["label"] = node.visible ? node.label : "";
                nodeAttrs["width"] = QString::number(qMax(1.0, node.size.width()) / 72.0, 'f', 4);
                nodeAttrs["height"] = QString::number(qMax(1.0, node.size.height()) / 72.0, 'f', 4);
                if (!node.visible) {
                    nodeAttrs["style"] = "invis";
                }
                dot += QString("    %1%2;\n").arg(quote(node.id), attrs(nodeAttrs));
                clusteredNodes.insert(node.id);
                break;
            }
        }
        dot += "  }\n";
    }

    for (const auto &node : graph.nodes) {
        if (clusteredNodes.contains(node.id)) {
            continue;
        }
        QHash<QString, QString> nodeAttrs = node.attrs;
        nodeAttrs["id"] = node.id;
        nodeAttrs["label"] = node.visible ? node.label : "";
        nodeAttrs["width"] = QString::number(qMax(1.0, node.size.width()) / 72.0, 'f', 4);
        nodeAttrs["height"] = QString::number(qMax(1.0, node.size.height()) / 72.0, 'f', 4);
        if (!node.visible) {
            nodeAttrs["style"] = "invis";
        }
        dot += QString("  %1%2;\n").arg(quote(node.id), attrs(nodeAttrs));
    }

    for (const auto &rank : graph.sameRanks) {
        if (rank.nodeIds.isEmpty()) {
            continue;
        }
        dot += "  { rank=same;";
        for (const auto &nodeId : rank.nodeIds) {
            dot += " " + quote(nodeId) + ";";
        }
        dot += " }\n";
    }

    for (const auto &edge : graph.edges) {
        QHash<QString, QString> edgeAttrs = edge.attrs;
        edgeAttrs["id"] = edge.id;
        edgeAttrs["constraint"] = edge.constraint ? "true" : "false";
        if (!edge.label.isEmpty()) {
            edgeAttrs["xlabel"] = edge.label;
        }
        if (!edge.visible) {
            edgeAttrs["style"] = "invis";
            edgeAttrs["arrowhead"] = "none";
        }
        QString fromStr = quote(edge.from);
        if (!edge.fromPort.isEmpty()) {
            fromStr += ":" + quote(edge.fromPort);
        }
        QString toStr = quote(edge.to);
        if (!edge.toPort.isEmpty()) {
            toStr += ":" + quote(edge.toPort);
        }
        dot += QString("  %1 -> %2%3;\n").arg(fromStr, toStr, attrs(edgeAttrs));
    }

    dot += "}\n";
    return dot;
}

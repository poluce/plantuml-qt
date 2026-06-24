#include "DotBuilder.h"
#include <QSet>
#include <QStringList>
#include <functional>
#include <QHash>

namespace {
    struct ClusterTreeNode {
        QString id;
        QString label;
        QString color;
        QVector<QString> nodeIds;
        QVector<ClusterTreeNode*> children;

        ~ClusterTreeNode() {
            qDeleteAll(children);
        }
    };

    ClusterTreeNode* getOrCreateNode(const QString& fullPath, const QString& label, const QString& color,
                                     QHash<QString, ClusterTreeNode*>& nodeMap, ClusterTreeNode* root)
    {
        if (nodeMap.contains(fullPath)) {
            return nodeMap[fullPath];
        }

        int lastIdx = fullPath.lastIndexOf('.');
        QString parentPath = "";
        QString selfLabel = fullPath;
        if (lastIdx != -1) {
            parentPath = fullPath.left(lastIdx);
            selfLabel = fullPath.mid(lastIdx + 1);
        }

        ClusterTreeNode* parentNode = nullptr;
        if (parentPath.isEmpty()) {
            parentNode = root;
        } else {
            int grandLastIdx = parentPath.lastIndexOf('.');
            QString parentLabel = (grandLastIdx == -1) ? parentPath : parentPath.mid(grandLastIdx + 1);
            parentNode = getOrCreateNode(parentPath, parentLabel, "", nodeMap, root);
        }

        ClusterTreeNode* newNode = new ClusterTreeNode();
        newNode->id = fullPath;
        newNode->label = label.isEmpty() ? selfLabel : label;
        newNode->color = color;

        parentNode->children.append(newNode);
        nodeMap[fullPath] = newNode;
        return newNode;
    }
}

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

    ClusterTreeNode root;
    root.id = "";
    QHash<QString, ClusterTreeNode*> nodeMap;
    nodeMap[""] = &root;

    for (const auto &cluster : graph.clusters) {
        ClusterTreeNode* node = getOrCreateNode(cluster.id, cluster.label, cluster.color, nodeMap, &root);
        if (!cluster.label.isEmpty()) {
            node->label = cluster.label;
        }
        if (!cluster.color.isEmpty()) {
            node->color = cluster.color;
        }
        node->nodeIds = cluster.nodeIds;
    }

    QSet<QString> clusteredNodes;

    std::function<void(const ClusterTreeNode*, int)> writeClusterNode;
    writeClusterNode = [&](const ClusterTreeNode *node, int indentLevel) {
        bool isRoot = node->id.isEmpty();
        QString indent = QString(indentLevel * 2, ' ');
        QString innerIndent = indent + "  ";

        if (!isRoot) {
            dot += QString("%1subgraph %2 {\n").arg(indent, quote("cluster_" + node->id));
            if (!node->color.isEmpty()) {
                dot += QString("%1graph [id=%2, label=%3, margin=\"18\", color=\"#cccccc\", bgcolor=%4];\n")
                           .arg(innerIndent, quote(node->id), quote(node->label), quote(node->color));
            } else {
                dot += QString("%1graph [id=%2, label=%3, margin=\"18\", color=\"transparent\"];\n")
                           .arg(innerIndent, quote(node->id), quote(node->label));
            }

            // 输出当前包内的类图元节点
            for (const auto &nodeId : node->nodeIds) {
                for (const auto &gNode : graph.nodes) {
                    if (gNode.id != nodeId) continue;

                    QHash<QString, QString> nodeAttrs = gNode.attrs;
                    nodeAttrs["id"] = gNode.id;
                    nodeAttrs["label"] = gNode.visible ? gNode.label : "";
                    nodeAttrs["width"] = QString::number(qMax(1.0, gNode.size.width()) / 72.0, 'f', 4);
                    nodeAttrs["height"] = QString::number(qMax(1.0, gNode.size.height()) / 72.0, 'f', 4);
                    if (!gNode.visible) {
                        nodeAttrs["style"] = "invis";
                    }
                    dot += QString("%1%2%3;\n").arg(innerIndent, quote(gNode.id), attrs(nodeAttrs));
                    clusteredNodes.insert(gNode.id);
                    break;
                }
            }
        }

        // 递归输出子 subgraph
        for (const auto *child : node->children) {
            writeClusterNode(child, isRoot ? indentLevel : indentLevel + 1);
        }

        if (!isRoot) {
            dot += QString("%1}\n").arg(indent);
        }
    };

    writeClusterNode(&root, 1);

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

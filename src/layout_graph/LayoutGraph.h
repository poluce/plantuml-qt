#pragma once

#include <QHash>
#include <QSizeF>
#include <QString>
#include <QVector>
#include "../render_model/RenderDocument.h"
#include "../parser/SourceLocation.h"

enum class LayoutGraphDirection
{
    TopToBottom,
    LeftToRight
};

struct LayoutNode
{
    QString id;
    QString label;
    QSizeF size;
    RenderNodeKind renderKind = RenderNodeKind::ClassBox;
    QString clusterId;
    bool visible = true;
    SourceLocation location;
    QHash<QString, QString> attrs;
};

struct LayoutEdge
{
    QString id;
    QString from;
    QString to;
    QString fromPort;  // 子端口 (如类成员名)
    QString toPort;    // 子端口 (如类成员名)
    QString label;
    RenderEdgeKind renderKind = RenderEdgeKind::Association;
    bool visible = true;
    bool constraint = true;
    SourceLocation location;
    QHash<QString, QString> attrs;

    QString style;
    QString taillabel;
    QString headlabel;
};

struct LayoutCluster
{
    QString id;
    QString label;
    QString color;
    QVector<QString> nodeIds;
};

struct LayoutRank
{
    QVector<QString> nodeIds;
};

struct LayoutGraph
{
    LayoutGraphDirection direction = LayoutGraphDirection::TopToBottom;
    QVector<LayoutNode> nodes;
    QVector<LayoutEdge> edges;
    QVector<LayoutCluster> clusters;
    QVector<LayoutRank> sameRanks;
};

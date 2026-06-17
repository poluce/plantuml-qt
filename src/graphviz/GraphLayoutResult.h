#pragma once

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

struct GraphLayoutNode
{
    QString id;
    QRectF rect;
};

struct GraphLayoutCluster
{
    QString id;
    QRectF rect;
};

struct GraphLayoutEdge
{
    QString id;
    QString from;
    QString to;
    QString label;
    QVector<QPointF> points;
    QPointF labelPosition;
    bool hasLabelPosition = false;
};

struct GraphLayoutResult
{
    QRectF graphBounds;
    QHash<QString, GraphLayoutNode> nodesById;
    QHash<QString, GraphLayoutCluster> clustersById;
    QVector<GraphLayoutEdge> edges;
    QString errorMessage;
};

#include "DotJsonParser.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QRectF DotJsonParser::parseBoundingBox(const QString &value) const
{
    const QStringList parts = value.split(',');
    if (parts.size() != 4) {
        return {};
    }
    const double x0 = parts[0].toDouble();
    const double y0 = parts[1].toDouble();
    const double x1 = parts[2].toDouble();
    const double y1 = parts[3].toDouble();
    return QRectF(x0, y0, x1 - x0, y1 - y0);
}

QPointF DotJsonParser::parsePoint(const QString &value, double graphHeight) const
{
    const QStringList parts = value.split(',');
    if (parts.size() < 2) {
        return {};
    }
    return QPointF(parts[0].toDouble(), graphHeight - parts[1].toDouble());
}

QVector<QPointF> DotJsonParser::parseEdgePoints(const QString &value, double graphHeight) const
{
    QVector<QPointF> points;
    for (QString token : value.split(' ', Qt::SkipEmptyParts)) {
        if (token.startsWith("e,") || token.startsWith("s,")) {
            token = token.mid(2);
        }
        if (!token.contains(',')) {
            continue;
        }
        points.append(parsePoint(token, graphHeight));
    }
    return points;
}

GraphLayoutResult DotJsonParser::parse(const QByteArray &json) const
{
    GraphLayoutResult result;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.errorMessage = "Graphviz JSON 解析失败: " + parseError.errorString();
        qWarning() << "[Graphviz]" << result.errorMessage;
        return result;
    }

    const QJsonObject root = doc.object();
    const QRectF rawBounds = parseBoundingBox(root.value("bb").toString());
    result.graphBounds = QRectF(0, 0, rawBounds.width(), rawBounds.height());
    const double graphHeight = rawBounds.height();

    QHash<int, QString> gvidToId;
    const QJsonArray objects = root.value("objects").toArray();
    for (const auto &value : objects) {
        const QJsonObject object = value.toObject();
        const QString name = object.value("name").toString();
        const QString id = object.value("id").toString(name);
        if (object.contains("_gvid")) {
            gvidToId[object.value("_gvid").toInt()] = id;
        }

        if (object.contains("nodes") && object.contains("bb")) {
            GraphLayoutCluster cluster;
            cluster.id = object.value("id").toString(name);
            QRectF bb = parseBoundingBox(object.value("bb").toString());
            cluster.rect = QRectF(bb.x(), graphHeight - bb.y() - bb.height(), bb.width(), bb.height());
            result.clustersById[cluster.id] = cluster;
            continue;
        }

        if (!object.contains("pos")) {
            continue;
        }
        const QPointF center = parsePoint(object.value("pos").toString(), graphHeight);
        const double width = object.value("width").toString().toDouble() * 72.0;
        const double height = object.value("height").toString().toDouble() * 72.0;

        GraphLayoutNode node;
        node.id = id;
        node.rect = QRectF(center.x() - width / 2.0, center.y() - height / 2.0, width, height);
        result.nodesById[node.id] = node;
    }

    const QJsonArray edges = root.value("edges").toArray();
    for (const auto &value : edges) {
        const QJsonObject object = value.toObject();
        GraphLayoutEdge edge;
        edge.id = object.value("id").toString();
        edge.from = gvidToId.value(object.value("tail").toInt());
        edge.to = gvidToId.value(object.value("head").toInt());
        edge.label = object.value("xlabel").toString(object.value("label").toString());
        edge.points = parseEdgePoints(object.value("pos").toString(), graphHeight);
        if (object.contains("xlp")) {
            edge.labelPosition = parsePoint(object.value("xlp").toString(), graphHeight);
            edge.hasLabelPosition = true;
        } else if (object.contains("lp")) {
            edge.labelPosition = parsePoint(object.value("lp").toString(), graphHeight);
            edge.hasLabelPosition = true;
        }
        result.edges.append(edge);
    }

    qDebug() << "[Graphviz] JSON 解析完成:"
             << "nodes" << result.nodesById.size()
             << "clusters" << result.clustersById.size()
             << "edges" << result.edges.size()
             << "bounds" << result.graphBounds;

    return result;
}

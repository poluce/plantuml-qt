#pragma once

#include <QByteArray>
#include "GraphLayoutResult.h"

class DotJsonParser
{
public:
    GraphLayoutResult parse(const QByteArray &json) const;

private:
    QRectF parseBoundingBox(const QString &value) const;
    QPointF parsePoint(const QString &value, double graphHeight) const;
    QVector<QPointF> parseEdgePoints(const QString &value, double graphHeight) const;
};

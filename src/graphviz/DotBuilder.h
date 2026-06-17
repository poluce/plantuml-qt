#pragma once

#include <QString>
#include "../layout_graph/LayoutGraph.h"

class DotBuilder
{
public:
    QString build(const LayoutGraph &graph) const;

private:
    QString quote(const QString &value) const;
    QString attrs(const QHash<QString, QString> &values) const;
};

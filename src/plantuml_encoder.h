#pragma once

#include <QObject>
#include <QString>

class PlantUmlEncoder : public QObject
{
    Q_OBJECT
public:
    explicit PlantUmlEncoder(QObject *parent = nullptr);

    // 将 PlantUML 文本编码为官方规范格式
    QString encode(const QString &text) const;
};

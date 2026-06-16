#pragma once

#include <QString>
#include <QVector>
#include "Token.h"

class PumlLexer
{
public:
    explicit PumlLexer(const QString &source);

    // 进行词法扫描，返回 Token 序列
    QVector<Token> tokenize();

private:
    QString m_source;
};

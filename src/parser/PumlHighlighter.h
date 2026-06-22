#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>

class PumlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit PumlHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureGroup = 0;
    };
    QVector<HighlightRule> m_rules;

    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_arrowFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
};

#include "PumlHighlighter.h"

PumlHighlighter::PumlHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // 1. 起止指令与状态图节点高亮样式 (猩红色，加粗)
    QTextCharFormat commandFormat;
    commandFormat.setForeground(QColor("#be123c"));
    commandFormat.setFontWeight(QFont::Bold);
    const QStringList commandPatterns = {
        R"(@startuml\b)", R"(@enduml\b)", R"(\[\*\])"
    };
    for (const QString &pattern : commandPatterns) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = commandFormat;
        m_rules.append(rule);
    }

    // 2. 预处理指令高亮样式 (橘红色，加粗)
    QTextCharFormat preprocFormat;
    preprocFormat.setForeground(QColor("#ea580c"));
    preprocFormat.setFontWeight(QFont::Bold);
    HighlightRule preprocRule;
    preprocRule.pattern = QRegularExpression(R"(!\w+)");
    preprocRule.format = preprocFormat;
    m_rules.append(preprocRule);

    // 3. 实体声明关键字高亮样式 (黄绿色，加粗 - 高度还原图片配色)
    m_keywordFormat.setForeground(QColor("#65a30d")); // 黄绿/橄榄绿
    m_keywordFormat.setFontWeight(QFont::Bold);
    const QStringList entityPatterns = {
        R"(\bclass\b)", R"(\binterface\b)", R"(\benum\b)", R"(\babstract\b)",
        R"(\bstruct\b)", R"(\bannotation\b)", R"(\bprotocol\b)", R"(\bmap\b)",
        R"(\bparticipant\b)", R"(\bactor\b)", R"(\bboundary\b)", R"(\bcontrol\b)",
        R"(\bentity\b)", R"(\bdatabase\b)", R"(\bcollections\b)", R"(\bqueue\b)",
        R"(\bstack\b)"
    };
    for (const QString &pattern : entityPatterns) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_keywordFormat;
        m_rules.append(rule);
    }

    // 4. 方法名与修饰符高亮样式 (金黄色/金橙色 - 高度还原图片配色)
    QTextCharFormat methodFormat;
    methodFormat.setForeground(QColor("#ca8a04")); // 金黄色
    methodFormat.setFontWeight(QFont::Bold);
    
    // 4.1 匹配方法名 (后面跟着小括号的标识符)
    HighlightRule methodRule;
    methodRule.pattern = QRegularExpression(R"(\b[a-zA-Z_]\w*(?=\s*\())");
    methodRule.format = methodFormat;
    m_rules.append(methodRule);

    // 4.2 匹配修饰符符标 (如 +, -, #, ~)
    HighlightRule modifierRule;
    modifierRule.pattern = QRegularExpression(R"([+\-#~])");
    modifierRule.format = methodFormat;
    m_rules.append(modifierRule);

    // 5. 数据类型与类名高亮样式 (紫红色/洋红色 - 高度还原图片配色)
    QTextCharFormat typeFormat;
    typeFormat.setForeground(QColor("#a21caf")); // 紫红色
    typeFormat.setFontWeight(QFont::Bold);

    // 5.1 匹配冒号后面的数据类型 (如 -m_id : QString 捕获 QString，或者 QVector<ThermalDataPoint> 整体)
    HighlightRule typeRule;
    typeRule.pattern = QRegularExpression(R"(:\s*([a-zA-Z_]\w*(?:<[a-zA-Z_]\w*>)?\b))");
    typeRule.format = typeFormat;
    typeRule.captureGroup = 1; // 仅高亮捕获组 1 里面的类型名
    m_rules.append(typeRule);

    // 5.2 匹配方法括号内部的参数类型 (如 setType(CurveType) 捕获 CurveType)
    HighlightRule paramTypeRule;
    paramTypeRule.pattern = QRegularExpression(R"(\(\s*([a-zA-Z_]\w*)\s*\))");
    paramTypeRule.format = typeFormat;
    paramTypeRule.captureGroup = 1; // 仅高亮捕获组 1 里面的参数类型
    m_rules.append(paramTypeRule);

    // 5.3 匹配基础数据类型
    HighlightRule basicTypeRule;
    basicTypeRule.pattern = QRegularExpression(R"(\b(?:void|int|double|float|string|bool)\b)");
    basicTypeRule.format = typeFormat;
    m_rules.append(basicTypeRule);

    // 5.4 匹配被声明的类名/图元实体名 (如 class ThermalDataPoint 捕获 ThermalDataPoint)
    HighlightRule declaredClassNameRule;
    declaredClassNameRule.pattern = QRegularExpression(R"(\b(?:class|interface|enum|abstract|struct|annotation|protocol|map|participant|actor|boundary|control|entity|database)\s+([a-zA-Z_]\w*))");
    declaredClassNameRule.format = typeFormat;
    declaredClassNameRule.captureGroup = 1;
    m_rules.append(declaredClassNameRule);

    // 5.5 匹配被起别名的图元名 (如 as AliasName 捕获 AliasName)
    HighlightRule aliasNameRule;
    aliasNameRule.pattern = QRegularExpression(R"(\bas\s+([a-zA-Z_]\w*))");
    aliasNameRule.format = typeFormat;
    aliasNameRule.captureGroup = 1;
    m_rules.append(aliasNameRule);

    // 6. 逻辑控制与时序/活动图流程指令 (天蓝色，加粗)
    QTextCharFormat controlFormat;
    controlFormat.setForeground(QColor("#0284c7"));
    controlFormat.setFontWeight(QFont::Bold);
    const QStringList controlPatterns = {
        R"(\balt\b)", R"(\belse\b)", R"(\bopt\b)", R"(\bloop\b)", R"(\bend\b)",
        R"(\bpar\b)", R"(\bcritical\b)", R"(\bgroup\b)", R"(\break\b)", R"(\bref\b)",
        R"(\bnote\b)", R"(\bover\b)", R"(\breturn\b)", R"(\bcreate\b)", R"(\bdestroy\b)",
        R"(\bautonumber\b)", R"(\bnewpage\b)", R"(\bactivate\b)", R"(\bdeactivate\b)",
        R"(\bstart\b)", R"(\bstop\b)", R"(\bif\b)", R"(\bthen\b)", R"(\belseif\b)",
        R"(\bendif\b)", R"(\bswitch\b)", R"(\bcase\b)", R"(\bfork\b)", R"(\bjoin\b)",
        R"(\bsplit\b)", R"(\bagain\b)", R"(\bkill\b)", R"(\bdetach\b)", R"(\brepeat\b)",
        R"(\bwhile\b)", R"(\bis\b)", R"(\bbackward\b)"
    };
    for (const QString &pattern : controlPatterns) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = controlFormat;
        m_rules.append(rule);
    }

    // 7. 布局/容器修饰词 (深粉色，加粗)
    QTextCharFormat decorFormat;
    decorFormat.setForeground(QColor("#db2777"));
    decorFormat.setFontWeight(QFont::Bold);
    const QStringList decorPatterns = {
        R"(\bpackage\b)", R"(\bnamespace\b)", R"(\bfolder\b)", R"(\bnode\b)",
        R"(\bcomponent\b)", R"(\busecase\b)", R"(\bstate\b)", R"(\bframe\b)",
        R"(\bcloud\b)", R"(\bas\b)", R"(\bskinparam\b)", R"(\btitle\b)",
        R"(\bscale\b)", R"(\bshow\b)", R"(\bhide\b)"
    };
    for (const QString &pattern : decorPatterns) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = decorFormat;
        m_rules.append(rule);
    }

    // 8. 继承与万能关系连线高亮样式 (靛蓝色，加粗)
    m_arrowFormat.setForeground(QColor("#2563eb"));
    m_arrowFormat.setFontWeight(QFont::Bold);
    HighlightRule arrowRule;
    arrowRule.pattern = QRegularExpression(R"((?:<\|?|x|o)?[-.]+(?:\|?>|x|o)?)");
    arrowRule.format = m_arrowFormat;
    m_rules.append(arrowRule);

    // 9. 字符串高亮样式 (绿色)
    m_stringFormat.setForeground(QColor("#16a34a"));
    HighlightRule stringRule;
    stringRule.pattern = QRegularExpression(R"("[^"\\]*(\\.[^"\\]*)*")");
    stringRule.format = m_stringFormat;
    m_rules.append(stringRule);

    // 10. 单行注释高亮样式 (灰色/斜体)
    m_commentFormat.setForeground(QColor("#9ca3af"));
    m_commentFormat.setFontItalic(true);
    HighlightRule commentRule;
    commentRule.pattern = QRegularExpression(R"((?:^|\s)'.*$)");
    commentRule.format = m_commentFormat;
    m_rules.append(commentRule);
}

void PumlHighlighter::highlightBlock(const QString &text)
{
    // 1. 应用单行高亮规则 (支持捕获组精准着色)
    for (const HighlightRule &rule : m_rules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            int start = match.capturedStart(rule.captureGroup);
            int length = match.capturedLength(rule.captureGroup);
            if (start >= 0 && length > 0) {
                setFormat(start, length, rule.format);
            }
        }
    }

    // 2. 跨行多行注释的处理 (增量渲染状态机)
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() == 1) {
        startIndex = 0;
    } else {
        startIndex = text.indexOf(QRegularExpression(R"(/\')"));
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch match = QRegularExpression(R"(\'/)").match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + match.capturedLength();
        }
        setFormat(startIndex, commentLength, m_commentFormat);
        if (endIndex == -1) {
            break;
        }
        startIndex = text.indexOf(QRegularExpression(R"(/\')"), startIndex + commentLength);
    }
}

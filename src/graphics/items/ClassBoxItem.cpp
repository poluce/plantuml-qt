#include "ClassBoxItem.h"
#include "RelationItem.h"
#include "NoteLineItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ClassBoxItem::ClassBoxItem(const RenderNode &node, const RenderTheme &theme)
    : m_node(node)
    , m_theme(theme)
    , m_initialPos(node.rect.topLeft())
    , m_fillColor(theme.surfaceColor)
    , m_borderColor(theme.primaryColor)
    , m_penWidth(theme.lineWidth)
    , m_penStyle(Qt::SolidLine)
{
    if (m_node.metaType == "point") {
        setAcceptHoverEvents(false);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
        setFlag(QGraphicsItem::ItemIsMovable, false);
    } else {
        setAcceptHoverEvents(true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemIsMovable, true);
    }
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
    // 解析 style 字符串属性
    QStringList parts = node.style.split(QRegularExpression("[;,]"), Qt::SkipEmptyParts);
    for (const auto &part : parts) {
        QString trimmed = part.trimmed();
        if (trimmed.isEmpty()) continue;
        
        if (trimmed.startsWith("back:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(5).trimmed();
            if (QColor::isValidColorName(col)) m_fillColor = QColor(col);
        } else if (trimmed.startsWith("fill=", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(5).trimmed();
            if (QColor::isValidColorName(col)) m_fillColor = QColor(col);
        } else if (trimmed.startsWith("fillcolor=", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(10).trimmed();
            if (QColor::isValidColorName(col)) m_fillColor = QColor(col);
        } else if (trimmed.startsWith('#')) {
            if (QColor::isValidColorName(trimmed)) m_fillColor = QColor(trimmed);
        }

        if (trimmed.startsWith("border.color:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(13).trimmed();
            if (QColor::isValidColorName(col)) m_borderColor = QColor(col);
        } else if (trimmed.startsWith("color=", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(6).trimmed();
            if (QColor::isValidColorName(col)) m_borderColor = QColor(col);
        } else if (trimmed.startsWith("line:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(5).trimmed();
            if (QColor::isValidColorName(col)) m_borderColor = QColor(col);
        } else if (trimmed.startsWith("border:", Qt::CaseInsensitive)) {
            QString col = trimmed.mid(7).trimmed();
            if (QColor::isValidColorName(col)) m_borderColor = QColor(col);
        }

        if (trimmed.startsWith("penwidth=", Qt::CaseInsensitive)) {
            m_penWidth = trimmed.mid(9).toDouble();
        } else if (trimmed == "bold" || trimmed.contains("bold", Qt::CaseInsensitive)) {
            m_penWidth = theme.lineWidth * 2.0;
        }

        if (trimmed.startsWith("border.style:", Qt::CaseInsensitive)) {
            QString styleStr = trimmed.mid(13).trimmed().toLower();
            if (styleStr == "dashed") m_penStyle = Qt::DashLine;
            else if (styleStr == "dotted") m_penStyle = Qt::DotLine;
        } else if (trimmed == "dashed" || trimmed.contains("dashed", Qt::CaseInsensitive)) {
            m_penStyle = Qt::DashLine;
        } else if (trimmed == "dotted" || trimmed.contains("dotted", Qt::CaseInsensitive)) {
            m_penStyle = Qt::DotLine;
        }
    }

    // 双重防御撑开机制：物理测量最小宽高
    double W_min = 160.0;
    double H_min = 70.0;

    if (m_node.kind != RenderNodeKind::Note && m_node.metaType != "json" &&
        m_node.metaType != "circle" && m_node.metaType != "()" &&
        m_node.metaType != "diamond" && m_node.metaType != "<>") {
        
        QFont nameFont("Arial", 10);
        nameFont.setBold(true);
        QFontMetrics fmName(nameFont);
        double nameW = fmName.horizontalAdvance(node.displayName.isEmpty() ? node.id : node.displayName);
        double stereoW = 0.0;
        if (!node.stereotype.isEmpty()) {
            QFont stereoFont("Arial", 8);
            stereoFont.setItalic(true);
            QFontMetrics fmStereo(stereoFont);
            QString topText = node.stereotype;
            if (!topText.startsWith("<<")) topText = "<<" + topText;
            if (!topText.endsWith(">>")) topText = topText + ">>";
            stereoW = fmStereo.horizontalAdvance(topText);
        } else if (node.metaType == "interface" || node.metaType == "enum") {
            QFont stereoFont("Arial", 8);
            stereoFont.setItalic(true);
            QFontMetrics fmStereo(stereoFont);
            QString topText = (node.metaType == "interface") ? "<<interface>>" : "<<enumeration>>";
            stereoW = fmStereo.horizontalAdvance(topText);
        }
        double W_head = qMax(nameW, stereoW) + 48.0;

        QFont memberFont("Arial", 9);
        QFontMetrics fmMember(memberFont);
        
        QFont titleFont("Arial", 8);
        titleFont.setItalic(true);
        QFontMetrics fmTitle(titleFont);
        
        double maxMemberW = 0.0;
        double totalMemberH = 0.0;
        
        for (const auto &member : node.members) {
            if (member.isSeparator) {
                totalMemberH += member.separatorText.isEmpty() ? 10.0 : 18.0;
                if (!member.separatorText.isEmpty()) {
                    double sepW = fmTitle.horizontalAdvance(member.separatorText) + 20.0;
                    maxMemberW = qMax(maxMemberW, sepW);
                }
            } else {
                totalMemberH += 18.0;
                QFont curFont = memberFont;
                if (member.isStatic) curFont.setUnderline(true);
                if (member.isAbstract) curFont.setItalic(true);
                QFontMetrics fmCur(curFont);
                
                QString cleanText = member.cleanText;
                if (cleanText.isEmpty()) {
                    cleanText = member.rawText;
                    if (cleanText.startsWith('+') || cleanText.startsWith('-') || cleanText.startsWith('#') || cleanText.startsWith('~')) {
                        cleanText = cleanText.mid(1).trimmed();
                    }
                    cleanText.replace("{static}", "");
                    cleanText.replace("{abstract}", "");
                    cleanText = cleanText.trimmed();
                }
                
                double memberW = 10.0 + 16.0 + 4.0 + fmCur.horizontalAdvance(cleanText) + 10.0;
                maxMemberW = qMax(maxMemberW, memberW);
            }
        }
        
        W_min = qMax(160.0, qMax(W_head, maxMemberW));
        H_min = 35.0 + 12.0 + (node.members.isEmpty() ? 18.0 : totalMemberH);
    } else if (m_node.metaType == "json") {
        double totalMemberH = node.members.size() * 18.0;
        H_min = 12.0 + 22.0 + totalMemberH + 12.0;
    }

    double actualW = 0.0;
    double actualH = 0.0;
    if (m_node.metaType == "point") {
        actualW = 1.0;
        actualH = 1.0;
    } else {
        actualW = qMax(node.rect.width(), W_min);
        actualH = qMax(node.rect.height(), H_min);
    }

    // 将卡片的绝对坐标转换为局部坐标绘制，使用 setPos 决定其场景空间位置，实现拖拽自适应
    setPos(node.rect.topLeft());
    m_node.rect = QRectF(0, 0, actualW, actualH);
}

QRectF ClassBoxItem::boundingRect() const
{
    if (m_node.metaType == "point") {
        return QRectF(m_node.rect.x() - 1, m_node.rect.y() - 1, 2, 2);
    }
    const QRectF &rect = m_node.rect;
    
    // 留出 6 像素的安全包围描边余量
    return QRectF(rect.x() - 6, rect.y() - 6, rect.width() + 12, rect.height() + 12);
}

void ClassBoxItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    if (m_node.metaType == "point") {
        return;
    }
    
    const QRectF &rect = m_node.rect;
    bool isHovered = option->state & QStyle::State_MouseOver;
    bool isSelected = option->state & QStyle::State_Selected;
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    // 1. 如果选中，在卡片外部画一圈淡雅的靛蓝发光外环
    if (isSelected) {
        QPen selectPen(QColor(99, 102, 241, 120), 4.0);
        painter->setPen(selectPen);
        painter->setBrush(Qt::NoBrush);
        
        if (m_node.metaType == "circle" || m_node.metaType == "()") {
            QPointF center = rect.center();
            painter->drawEllipse(QRectF(center.x() - 17.0, center.y() - 17.0 - 10.0, 34.0, 34.0));
        } else if (m_node.metaType == "diamond" || m_node.metaType == "<>") {
            QPointF center(rect.center().x(), rect.center().y() - 10.0);
            QPolygonF selPoly;
            selPoly << QPointF(center.x(), center.y() - 14.0)
                    << QPointF(center.x() + 14.0, center.y())
                    << QPointF(center.x(), center.y() + 14.0)
                    << QPointF(center.x() - 14.0, center.y());
            painter->drawPolygon(selPoly);
        } else {
            painter->drawRoundedRect(rect.adjusted(-2, -2, 2, 2), m_theme.nodeRadius + 1, m_theme.nodeRadius + 1);
        }
    }
    
    // 2. 根据不同的元数据类型进行分块绘制
    if (m_node.kind == RenderNodeKind::Note) {
        // ================= Note 气泡框绘制 =================
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        // 设置 Note 风格色彩（经典淡黄色与琥珀色边框）
        QColor noteBg = QColor("#fffbeb"); // 极浅黄背景
        QColor noteBorder = QColor("#d97706"); // 琥珀色边框
        if (isHovered) {
            noteBg = QColor("#fef3c7");
        }

        if (m_fillColor != m_theme.surfaceColor) {
            noteBg = m_fillColor;
        }
        if (m_borderColor != m_theme.primaryColor) {
            noteBorder = m_borderColor;
        }

        painter->setPen(QPen(noteBorder, m_penWidth, m_penStyle));
        painter->setBrush(QBrush(noteBg));

        // 绘制折角边框路径
        QPainterPath path;
        const double fold = 10.0;
        path.moveTo(rect.left(), rect.top());
        path.lineTo(rect.right() - fold, rect.top());
        path.lineTo(rect.right(), rect.top() + fold);
        path.lineTo(rect.right(), rect.bottom());
        path.lineTo(rect.left(), rect.bottom());
        path.closeSubpath();
        painter->drawPath(path);

        // 绘制折折角背面小三角
        QPainterPath foldPath;
        foldPath.moveTo(rect.right() - fold, rect.top());
        foldPath.lineTo(rect.right() - fold, rect.top() + fold);
        foldPath.lineTo(rect.right(), rect.top() + fold);
        painter->setBrush(QBrush(noteBg.darker(115))); // 比背景稍暗的折角阴影
        painter->drawPath(foldPath);

        // 绘制 Note 文本
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(false);
        painter->setFont(font);

        // 预留内衬边距并支持自动换行
        QRectF textRect = rect.adjusted(10, 8, -10, -8);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_node.displayName);

        painter->restore();
    }
    else if (m_node.metaType == "json") {
        // ================= JSON 卡片绘制 =================
        // 画统一底色的圆角矩形
        painter->setPen(QPen(m_borderColor, m_penWidth, m_penStyle));
        painter->setBrush(QBrush(m_fillColor));
        painter->drawRoundedRect(rect, m_theme.nodeRadius, m_theme.nodeRadius);

        // 绘制标题
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(10);
        font.setBold(true);
        painter->setFont(font);

        double yOff = rect.y() + 12.0;
        QRectF titleRect(rect.x() + 10.0, yOff, rect.width() - 20.0, 20.0);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
        yOff += 22.0;

        // 绘制成员列表
        font.setBold(false);
        font.setPointSize(9);
        painter->setFont(font);
        painter->setPen(m_theme.onSurfaceMutedColor);

        double memberLineHeight = 18.0;
        for (const auto &member : m_node.members) {
            QRectF labelRect(rect.x() + 10.0, yOff, rect.width() - 20.0, memberLineHeight);
            painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, member.rawText);
            yOff += memberLineHeight;
        }
    }
    else if (m_node.metaType == "circle" || m_node.metaType == "()") {
        // ================= 圆圈棒棒糖绘制 =================
        QPointF center(rect.center().x(), rect.center().y() - 10.0);
        QRectF circleRect(center.x() - 15.0, center.y() - 15.0, 30.0, 30.0);

        painter->setPen(QPen(m_borderColor, m_penWidth, m_penStyle));
        painter->setBrush(QBrush(m_fillColor));
        painter->drawEllipse(circleRect);

        // 文字绘制
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(true);
        painter->setFont(font);

        QRectF textRect(rect.x(), center.y() + 18.0, rect.width(), 20.0);
        painter->drawText(textRect, Qt::AlignCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
    }
    else if (m_node.metaType == "diamond" || m_node.metaType == "<>") {
        // ================= 菱形绘制 =================
        QPointF center(rect.center().x(), rect.center().y() - 10.0);
        QPolygonF polygon;
        polygon << QPointF(center.x(), center.y() - 12.0)
                << QPointF(center.x() + 12.0, center.y())
                << QPointF(center.x(), center.y() + 12.0)
                << QPointF(center.x() - 12.0, center.y());

        painter->setPen(QPen(m_borderColor, m_penWidth, m_penStyle));
        painter->setBrush(QBrush(m_fillColor));
        painter->drawPolygon(polygon);

        // 文字绘制
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(true);
        painter->setFont(font);

        QRectF textRect(rect.x(), center.y() + 16.0, rect.width(), 20.0);
        painter->drawText(textRect, Qt::AlignCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
    }
    else {
        // ================= 普通类节点卡片绘制 =================
        const double headerHeight = 35.0;
        
        // 2. 绘制类卡片大背景
        QPen borderPen(m_borderColor, m_penWidth, m_penStyle);
        painter->setPen(borderPen);
        painter->setBrush(QBrush(m_fillColor));
        painter->drawRoundedRect(rect, m_theme.nodeRadius, m_theme.nodeRadius);
        
        // 3. 绘制上半部的类名头部背景区
        QColor headerFill = isHovered ? QColor("#c7d2fe") : m_theme.primaryLightColor;
        if (m_fillColor != m_theme.surfaceColor) {
            headerFill = m_fillColor.darker(110);
        }
        painter->setBrush(QBrush(headerFill));
        
        QPainterPath headerPath;
        QRectF headRect(rect.x(), rect.y(), rect.width(), headerHeight);
        headerPath.addRoundedRect(headRect, m_theme.nodeRadius, m_theme.nodeRadius);
        
        QRectF clipRect(rect.x(), rect.y() + headerHeight - 8.0, rect.width(), 8.0);
        QPainterPath clipPath;
        clipPath.addRect(clipRect);
        headerPath = headerPath.united(clipPath);
        
        painter->drawPath(headerPath);
        
        // 4. 绘制类名区与成员区分割实线
        painter->setPen(QPen(m_theme.borderColor, m_theme.lineWidth));
        painter->drawLine(QPointF(rect.x(), rect.y() + headerHeight), QPointF(rect.x() + rect.width(), rect.y() + headerHeight));
        
        // 5. 绘制圆形 Spot
        double spotSize = 14.0;
        QRectF spotRect(rect.x() + 8.0, rect.y() + (headerHeight - spotSize) / 2.0, spotSize, spotSize);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        
        QColor spotBgColor;
        QString spotText = "C";
        
        if (m_node.metaType.compare("interface", Qt::CaseInsensitive) == 0) {
            spotBgColor = QColor(241, 196, 15); // 金色
            spotText = "I";
        } else if (m_node.metaType.compare("enum", Qt::CaseInsensitive) == 0) {
            spotBgColor = QColor(46, 204, 113); // 绿色
            spotText = "E";
        } else if (m_node.metaType.compare("abstract", Qt::CaseInsensitive) == 0 ||
                   m_node.metaType.compare("abstract class", Qt::CaseInsensitive) == 0) {
            spotBgColor = QColor(230, 126, 34); // 橙色
            spotText = "A";
        } else {
            spotBgColor = QColor(52, 152, 219); // 蓝色
            spotText = "C";
        }
        
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(spotBgColor));
        painter->drawEllipse(spotRect);
        
        painter->setPen(Qt::white);
        QFont spotFont = painter->font();
        spotFont.setPointSize(8);
        spotFont.setBold(true);
        painter->setFont(spotFont);
        painter->drawText(spotRect, Qt::AlignCenter, spotText);
        painter->restore();
        
        // 6. 绘制类名与元类/Stereotype修饰文本
        painter->setPen(m_theme.onSurfaceColor);
        QFont font = painter->font();
        font.setPointSize(10);
        font.setBold(true);
        painter->setFont(font);
        
        bool hasTopLabel = !m_node.stereotype.isEmpty() || (m_node.metaType == "interface" || m_node.metaType == "enum");
        if (hasTopLabel) {
            QString topText;
            if (!m_node.stereotype.isEmpty()) {
                topText = m_node.stereotype;
                if (!topText.startsWith("<<")) topText = "<<" + topText;
                if (!topText.endsWith(">>")) topText = topText + ">>";
            } else {
                topText = (m_node.metaType == "interface") ? "<<interface>>" : "<<enumeration>>";
            }
            
            QFont metaFont = font;
            metaFont.setPointSize(8);
            metaFont.setBold(false);
            metaFont.setItalic(true);
            painter->setFont(metaFont);
            
            QRectF metaRect(rect.x() + 24.0, rect.y() + 3, rect.width() - 32.0, 14);
            painter->drawText(metaRect, Qt::AlignCenter, topText);
            
            painter->setFont(font);
            QRectF nameRect(rect.x() + 24.0, rect.y() + 15, rect.width() - 32.0, 18);
            painter->drawText(nameRect, Qt::AlignCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
        } else {
            QRectF headTextRect(rect.x() + 24.0, rect.y(), rect.width() - 32.0, headerHeight);
            painter->drawText(headTextRect, Qt::AlignCenter, m_node.displayName.isEmpty() ? m_node.id : m_node.displayName);
        }
        
        // 7. 依次换行左对齐绘制类成员 (属性与方法)
        font.setPointSize(9);
        font.setBold(false);
        painter->setFont(font);
        painter->setPen(m_theme.onSurfaceMutedColor);
        
        QVector<double> memberYOffsets;
        {
            double yCurrent = rect.y() + headerHeight + 6.0;
            for (int i = 0; i < m_node.members.size(); ++i) {
                memberYOffsets.append(yCurrent);
                const auto &member = m_node.members[i];
                double h = 18.0;
                if (member.isSeparator) {
                    h = member.separatorText.isEmpty() ? 10.0 : 18.0;
                }
                yCurrent += h;
            }
        }
        
        for (int k = 0; k < m_node.members.size(); ++k) {
            double yOff = memberYOffsets[k];
            const auto &member = m_node.members[k];
            
            if (member.isSeparator) {
                painter->save();
                double lineH = member.separatorText.isEmpty() ? 10.0 : 18.0;
                double lineY = yOff + lineH / 2.0;
                
                QPen sepPen(m_theme.borderColor, 1.0);
                if (member.rawText.contains('.')) {
                    sepPen.setStyle(Qt::DotLine);
                } else if (member.rawText.contains("__")) {
                    sepPen.setStyle(Qt::SolidLine);
                    sepPen.setWidthF(2.0);
                } else if (member.rawText.contains('=')) {
                    sepPen.setStyle(Qt::SolidLine);
                } else {
                    sepPen.setStyle(Qt::SolidLine);
                }
                painter->setPen(sepPen);
                
                if (!member.separatorText.isEmpty()) {
                    QFont sepFont = painter->font();
                    sepFont.setPointSize(8);
                    sepFont.setItalic(true);
                    painter->setFont(sepFont);
                    painter->setPen(m_theme.onSurfaceMutedColor);
                    
                    QFontMetrics fm(sepFont);
                    double textW = fm.horizontalAdvance(member.separatorText);
                    
                    double X_pad = 4.0;
                    double P = 6.0;
                    double X_mid = rect.x() + rect.width() / 2.0;
                    double X_left = X_mid - textW / 2.0;
                    double X_right = X_mid + textW / 2.0;
                    
                    painter->save();
                    painter->setPen(sepPen);
                    if (member.rawText.contains('=')) {
                        if (X_left - P > rect.x() + X_pad) {
                            painter->drawLine(QPointF(rect.x() + X_pad, lineY - 1.5), QPointF(X_left - P, lineY - 1.5));
                            painter->drawLine(QPointF(rect.x() + X_pad, lineY + 1.5), QPointF(X_left - P, lineY + 1.5));
                        }
                        if (rect.x() + rect.width() - X_pad > X_right + P) {
                            painter->drawLine(QPointF(X_right + P, lineY - 1.5), QPointF(rect.x() + rect.width() - X_pad, lineY - 1.5));
                            painter->drawLine(QPointF(X_right + P, lineY + 1.5), QPointF(rect.x() + rect.width() - X_pad, lineY + 1.5));
                        }
                    } else {
                        if (X_left - P > rect.x() + X_pad) {
                            painter->drawLine(QPointF(rect.x() + X_pad, lineY), QPointF(X_left - P, lineY));
                        }
                        if (rect.x() + rect.width() - X_pad > X_right + P) {
                            painter->drawLine(QPointF(X_right + P, lineY), QPointF(rect.x() + rect.width() - X_pad, lineY));
                        }
                    }
                    painter->restore();
                    
                    QRectF labelRect(X_left, yOff, textW, lineH);
                    painter->drawText(labelRect, Qt::AlignCenter, member.separatorText);
                } else {
                    if (member.rawText.contains('=')) {
                        painter->drawLine(QPointF(rect.x() + 4.0, lineY - 1.5), QPointF(rect.x() + rect.width() - 4.0, lineY - 1.5));
                        painter->drawLine(QPointF(rect.x() + 4.0, lineY + 1.5), QPointF(rect.x() + rect.width() - 4.0, lineY + 1.5));
                    } else {
                        painter->drawLine(QPointF(rect.x() + 4.0, lineY), QPointF(rect.x() + rect.width() - 4.0, lineY));
                    }
                }
                painter->restore();
            } else {
                painter->save();
                QFont memberFont = painter->font();
                if (member.isStatic) {
                    memberFont.setUnderline(true);
                }
                if (member.isAbstract) {
                    memberFont.setItalic(true);
                }
                painter->setFont(memberFont);
                
                QString cleanText = member.cleanText;
                if (cleanText.isEmpty()) {
                    cleanText = member.rawText;
                    if (cleanText.startsWith('+') || cleanText.startsWith('-') || cleanText.startsWith('#') || cleanText.startsWith('~')) {
                        cleanText = cleanText.mid(1).trimmed();
                    }
                    cleanText.replace("{static}", "");
                    cleanText.replace("{abstract}", "");
                    cleanText = cleanText.trimmed();
                }
                
                double W_icon = 16.0;
                QRectF iconRect(rect.x() + 10.0, yOff, W_icon, 18.0);
                QRectF textRect(rect.x() + 30.0, yOff, rect.width() - 40.0, 18.0);
                
                if (!member.visibility.isEmpty()) {
                    painter->save();
                    painter->setRenderHint(QPainter::Antialiasing, true);
                    double size = 8.0;
                    QRectF shapeRect(iconRect.x() + (W_icon - size)/2.0, iconRect.y() + (18.0 - size)/2.0, size, size);
                    
                    if (member.visibility == "+") {
                        painter->setBrush(QBrush(QColor("#4ADE80")));
                        painter->setPen(QPen(QColor("#16A34A"), 1.0));
                        painter->drawEllipse(shapeRect);
                    } else if (member.visibility == "-") {
                        painter->setBrush(QBrush(QColor("#F87171")));
                        painter->setPen(QPen(QColor("#DC2626"), 1.0));
                        painter->drawRect(shapeRect);
                    } else if (member.visibility == "#") {
                        painter->setBrush(QBrush(QColor("#FBBF24")));
                        painter->setPen(QPen(QColor("#D97706"), 1.0));
                        QPolygonF diamond;
                        diamond << QPointF(shapeRect.center().x(), shapeRect.top())
                                << QPointF(shapeRect.right(), shapeRect.center().y())
                                << QPointF(shapeRect.center().x(), shapeRect.bottom())
                                << QPointF(shapeRect.left(), shapeRect.center().y());
                        painter->drawPolygon(diamond);
                    } else if (member.visibility == "~") {
                        painter->setBrush(QBrush(QColor("#60A5FA")));
                        painter->setPen(QPen(QColor("#2563EB"), 1.0));
                        QPolygonF triangle;
                        triangle << QPointF(shapeRect.center().x(), shapeRect.top())
                                 << QPointF(shapeRect.right(), shapeRect.bottom())
                                 << QPointF(shapeRect.left(), shapeRect.bottom());
                        painter->drawPolygon(triangle);
                    }
                    painter->restore();
                }
                
                painter->setPen(m_theme.onSurfaceColor);
                painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, cleanText);
                
                painter->restore();
            }
        }
    }
    
    painter->restore();
}

void ClassBoxItem::addEdge(QGraphicsItem *edge)
{
    if (!m_edges.contains(edge)) {
        m_edges.append(edge);
    }
}

void ClassBoxItem::updateEdges()
{
    for (auto *edge : m_edges) {
        if (auto *rel = dynamic_cast<RelationItem*>(edge)) {
            rel->trackNodes();
        } else if (auto *noteLine = dynamic_cast<NoteLineItem*>(edge)) {
            noteLine->trackNodes();
        }
    }
}

QVariant ClassBoxItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        updateEdges();
    }
    return QGraphicsItem::itemChange(change, value);
}

QPointF ClassBoxItem::portPoint(const QString &portId, const QPointF &fallback, BoxSide side) const
{
    // 0. 特殊处理菱形和圆形，使其连线能够精确吸附在其真实几何边缘上
    if (m_node.metaType == "diamond" || m_node.metaType == "<>" ||
        m_node.metaType == "circle" || m_node.metaType == "()") {
        
        QPointF localCenter(m_node.rect.center().x(), m_node.rect.center().y() - 10.0);
        double radius = (m_node.metaType == "diamond" || m_node.metaType == "<>") ? 12.0 : 15.0;
        
        if (side == BoxSide::Left) {
            return mapToScene(QPointF(localCenter.x() - radius, localCenter.y()));
        } else if (side == BoxSide::Right) {
            return mapToScene(QPointF(localCenter.x() + radius, localCenter.y()));
        } else if (side == BoxSide::Top) {
            return mapToScene(QPointF(localCenter.x(), localCenter.y() - radius));
        } else if (side == BoxSide::Bottom) {
            return mapToScene(QPointF(localCenter.x(), localCenter.y() + radius));
        }
        return mapToScene(localCenter);
    }
    // 1. 如果是 Note 节点，Y 轴强制对其侧边中心，X 轴物理贴边
    if (m_node.kind == RenderNodeKind::Note) {
        QRectF r = mapRectToScene(m_node.rect);
        double sceneY = mapToScene(QPointF(0, m_node.rect.height() / 2.0)).y();
        if (side == BoxSide::Left) {
            return QPointF(r.left(), sceneY);
        } else if (side == BoxSide::Right) {
            return QPointF(r.right(), sceneY);
        }
        return fallback;
    }

    // 2. 如果是 Class 且无指定端口，保持默认贴边 X 坐标，Y 轴由 preferred 决定
    if (portId.isEmpty() || !portId.startsWith("member_")) {
        QRectF r = mapRectToScene(m_node.rect);
        if (side == BoxSide::Left) {
            return QPointF(r.left(), fallback.y());
        } else if (side == BoxSide::Right) {
            return QPointF(r.right(), fallback.y());
        }
        return fallback;
    }

    // 3. 如果是 Class 且指定了特定的成员端口：Y 轴对准成员文字中线，X 轴物理贴边
    bool ok = false;
    int memberIdx = portId.mid(7).toInt(&ok);
    if (!ok || memberIdx < 0 || memberIdx >= m_node.members.size()) {
        return fallback;
    }
    
    // 使用不含描边外扩余量的物理 rect 计算场景坐标边缘，保证连线物理贴合
    QRectF r = mapRectToScene(m_node.rect);
    
    // 使用与 paint 完全一致的 Y 轴累加规则计算中线
    double yCurrent = 35.0 + 6.0; // headerHeight + padding
    QVector<double> memberYOffsets;
    QVector<double> memberHeights;
    for (int i = 0; i < m_node.members.size(); ++i) {
        memberYOffsets.append(yCurrent);
        double h = 18.0;
        if (m_node.members[i].isSeparator) {
            h = m_node.members[i].separatorText.isEmpty() ? 10.0 : 18.0;
        }
        memberHeights.append(h);
        yCurrent += h;
    }
    
    double localY = memberYOffsets[memberIdx] + memberHeights[memberIdx] / 2.0;
    double sceneY = mapToScene(QPointF(0, localY)).y();
    
    if (side == BoxSide::Left) {
        return QPointF(r.left(), sceneY);
    } else if (side == BoxSide::Right) {
        return QPointF(r.right(), sceneY);
    }
    return fallback;
}

void ClassBoxItem::initAssociation(ClassBoxItem *fromItem, ClassBoxItem *toItem)
{
    m_assocFromItem = fromItem;
    m_assocToItem = toItem;
    m_hasAssoc = true;
    
    if (!fromItem || !toItem) return;
    
    QPointF pa = fromItem->sceneBoundingRect().center();
    QPointF pb = toItem->sceneBoundingRect().center();
    QPointF pv = this->pos();
    
    QPointF ab = pb - pa;
    double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 > 1e-6) {
        m_assocT = ((pv.x() - pa.x()) * ab.x() + (pv.y() - pa.y()) * ab.y()) / len2;
        QPointF projection = pa + m_assocT * ab;
        m_assocD = pv - projection;
    } else {
        m_assocT = 0.5;
        m_assocD = QPointF(0, 0);
    }
}

void ClassBoxItem::updateAssocPosition()
{
    if (!m_hasAssoc || !m_assocFromItem || !m_assocToItem) return;
    
    QPointF pa = m_assocFromItem->sceneBoundingRect().center();
    QPointF pb = m_assocToItem->sceneBoundingRect().center();
    
    QPointF ab = pb - pa;
    QPointF projection = pa + m_assocT * ab;
    QPointF newPos = projection + m_assocD;
    
    if (qAbs(this->pos().x() - newPos.x()) < 1e-4 && qAbs(this->pos().y() - newPos.y()) < 1e-4) {
        return;
    }
    
    this->setPos(newPos);
}

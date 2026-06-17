#include "PumlLexer.h"

PumlLexer::PumlLexer(const QString &source)
    : m_source(source)
{
}

QVector<Token> PumlLexer::tokenize()
{
    QVector<Token> tokens;
    int len = m_source.length();
    int i = 0;
    int line = 1;
    int col = 1;
    
    while (i < len) {
        QChar c = m_source[i];
        
        // 0. 处理单行注释 '
        if (c == '\'') {
            while (i < len && m_source[i] != '\n' && m_source[i] != '\r') {
                i++;
            }
            continue;
        }
        
        // 1. 处理换行符 (CRLF/LF)
        if (c == '\n') {
            Token t;
            t.type = TokenType::Newline;
            t.location = {line, col, 1};
            tokens.append(t);
            line++;
            col = 1;
            i++;
            continue;
        }
        if (c == '\r') {
            Token t;
            t.type = TokenType::Newline;
            t.location = {line, col, 1};
            tokens.append(t);
            i++;
            if (i < len && m_source[i] == '\n') {
                i++;
            }
            line++;
            col = 1;
            continue;
        }
        
        // 2. 忽略空白字符
        if (c == ' ' || c == '\t') {
            i++;
            col++;
            continue;
        }
        
        // 2.1 识别大括号 (类成员块界定)
        if (c == '{') {
            Token t;
            t.type = TokenType::LeftBrace;
            t.location = {line, col, 1};
            tokens.append(t);
            i++;
            col++;
            continue;
        }
        if (c == '}') {
            Token t;
            t.type = TokenType::RightBrace;
            t.location = {line, col, 1};
            tokens.append(t);
            i++;
            col++;
            continue;
        }
        
        // 2.2 识别关系连线 (通用前瞻探测)
        if (c == '<' || c == '*' || c == 'o' || c == '.' || c == '-') {
            int p = i;
            QString left = "";
            if (i + 1 < len && m_source[i] == '<' && m_source[i + 1] == '|') {
                left = "<|";
                p += 2;
            } else if (m_source[i] == '*') {
                left = "*";
                p += 1;
            } else if (m_source[i] == 'o') {
                left = "o";
                p += 1;
            }
            
            // 探测线体
            if (p < len && (m_source[p] == '-' || m_source[p] == '.')) {
                QChar lineChar = m_source[p];
                int n1 = 0;
                while (p < len && m_source[p] == lineChar) {
                    n1++;
                    p++;
                }
                
                QString directionStr = "";
                
                // 尝试匹配带方括号的方向，如 -[left]-
                if (p < len && m_source[p] == '[') {
                    int braceEnd = p + 1;
                    while (braceEnd < len && m_source[braceEnd] != ']' && m_source[braceEnd] != '\n' && m_source[braceEnd] != '\r') {
                        braceEnd++;
                    }
                    if (braceEnd < len && m_source[braceEnd] == ']') {
                        QString dirVal = m_source.mid(p + 1, braceEnd - p - 1).trimmed().toLower();
                        if (dirVal == "up" || dirVal == "down" || dirVal == "left" || dirVal == "right" ||
                            dirVal == "u" || dirVal == "d" || dirVal == "l" || dirVal == "r" ||
                            dirVal == "top" || dirVal == "bottom" || dirVal == "t" || dirVal == "b") {
                            
                            int postBrace = braceEnd + 1;
                            // 后面必须紧跟至少一个 lineChar
                            if (postBrace < len && m_source[postBrace] == lineChar) {
                                directionStr = dirVal;
                                p = postBrace;
                                while (p < len && m_source[p] == lineChar) {
                                    p++;
                                }
                            }
                        }
                    }
                }
                // 尝试匹配不带方括号的方向，如 -up-
                else if (directionStr.isEmpty() && p < len && m_source[p].isLetter()) {
                    int letterStart = p;
                    while (p < len && m_source[p].isLetter()) {
                        p++;
                    }
                    QString dirVal = m_source.mid(letterStart, p - letterStart).toLower();
                    if (dirVal == "up" || dirVal == "down" || dirVal == "left" || dirVal == "right" ||
                        dirVal == "u" || dirVal == "d" || dirVal == "l" || dirVal == "r" ||
                        dirVal == "top" || dirVal == "bottom" || dirVal == "t" || dirVal == "b") {
                        
                        // 后面必须紧跟至少一个 lineChar
                        if (p < len && m_source[p] == lineChar) {
                            directionStr = dirVal;
                            while (p < len && m_source[p] == lineChar) {
                                p++;
                            }
                        } else {
                            // 回退 p
                            p = letterStart;
                        }
                    } else {
                        // 回退 p
                        p = letterStart;
                    }
                }
                
                // 探测右侧标记
                QString right = "";
                if (p + 1 < len && m_source[p] == '|' && m_source[p + 1] == '>') {
                    right = "|>";
                    p += 2;
                } else if (p < len && m_source[p] == '>') {
                    right = ">";
                    p += 1;
                }
                
                // 判定是否构成合法连线并映射 TokenType
                TokenType detectedType = TokenType::Unknown;
                bool isValidRelation = false;
                
                if (left == "<|") {
                    if (lineChar == '-') {
                        detectedType = TokenType::Inherit;
                        isValidRelation = true;
                    } else if (lineChar == '.') {
                        detectedType = TokenType::Realization;
                        isValidRelation = true;
                    }
                } else if (left == "*") {
                    if (lineChar == '-') {
                        if (directionStr.isEmpty() && right.isEmpty()) {
                            if (n1 >= 2) {
                                detectedType = TokenType::Composition;
                                isValidRelation = true;
                            }
                        } else {
                            detectedType = TokenType::Composition;
                            isValidRelation = true;
                        }
                    }
                } else if (left == "o") {
                    if (lineChar == '-') {
                        if (directionStr.isEmpty() && right.isEmpty()) {
                            if (n1 >= 2) {
                                detectedType = TokenType::Aggregation;
                                isValidRelation = true;
                            }
                        } else {
                            detectedType = TokenType::Aggregation;
                            isValidRelation = true;
                        }
                    }
                } else { // left == ""
                    if (right == "|>") {
                        if (lineChar == '-') {
                            detectedType = TokenType::InheritRight;
                            isValidRelation = true;
                        } else if (lineChar == '.') {
                            detectedType = TokenType::RealizeRight;
                            isValidRelation = true;
                        }
                    } else if (right == ">") {
                        if (lineChar == '-') {
                            if (n1 == 1) {
                                detectedType = TokenType::Arrow;
                            } else {
                                detectedType = TokenType::DottedArrow;
                            }
                            isValidRelation = true;
                        } else if (lineChar == '.') {
                            detectedType = TokenType::Dependency;
                            isValidRelation = true;
                        }
                    }
                }
                
                if (isValidRelation) {
                    Token t;
                    t.type = detectedType;
                    t.value = m_source.mid(i, p - i);
                    t.location = {line, col, p - i};
                    
                    // 标准化方向并写入 direction
                    if (!directionStr.isEmpty()) {
                        if (directionStr == "up" || directionStr == "u" || directionStr == "top" || directionStr == "t") {
                            t.direction = "up";
                        } else if (directionStr == "down" || directionStr == "d" || directionStr == "bottom" || directionStr == "b") {
                            t.direction = "down";
                        } else if (directionStr == "left" || directionStr == "l") {
                            t.direction = "left";
                        } else if (directionStr == "right" || directionStr == "r") {
                            t.direction = "right";
                        }
                    }
                    
                    tokens.append(t);
                    col += (p - i);
                    i = p;
                    continue;
                }
            }
        }
        
        // 3. 处理以 @ 开头的特殊关键字 (@startuml, @enduml)
        if (c == '@') {
            int startCol = col;
            QString val;
            while (i < len && !m_source[i].isSpace()) {
                val.append(m_source[i]);
                i++;
                col++;
            }
            Token t;
            t.location = {line, startCol, static_cast<int>(val.length())};
            t.value = val;
            if (val.startsWith("@start")) {
                t.type = TokenType::StartUml;
            } else if (val.startsWith("@end")) {
                t.type = TokenType::EndUml;
            } else {
                t.type = TokenType::Unknown;
            }
            tokens.append(t);
            continue;
        }
        
        // 4. 遇到冒号，将冒号后面的行内非空文本全部打包为一个 String Token 捕获
        if (c == ':') {
            Token tColon;
            tColon.type = TokenType::Colon;
            tColon.location = {line, col, 1};
            tokens.append(tColon);
            i++;
            col++;
            
            // 跳过冒号后的行内空白
            while (i < len && m_source[i] != '\n' && m_source[i] != '\r' && (m_source[i] == ' ' || m_source[i] == '\t')) {
                i++;
                col++;
            }
            
            int startCol = col;
            QString strVal;
            while (i < len && m_source[i] != '\n' && m_source[i] != '\r') {
                strVal.append(m_source[i]);
                i++;
                col++;
            }
            
            // 剔除前后可能包裹的双引号
            strVal = strVal.trimmed();
            if (strVal.startsWith('\"') && strVal.endsWith('\"') && strVal.length() >= 2) {
                strVal = strVal.mid(1, strVal.length() - 2);
            }
            
            Token tStr;
            tStr.type = TokenType::String;
            tStr.value = strVal;
            tStr.location = {line, startCol, qMax(1, static_cast<int>(strVal.length()))};
            tokens.append(tStr);
            continue;
        }
        

        
        // 6. 识别带双引号的单词
        if (c == '\"') {
            int startCol = col;
            i++; // 跳过首部引号
            col++;
            QString quotedVal;
            while (i < len && m_source[i] != '\"' && m_source[i] != '\n' && m_source[i] != '\r') {
                quotedVal.append(m_source[i]);
                i++;
                col++;
            }
            if (i < len && m_source[i] == '\"') {
                i++; // 跳过闭合引号
                col++;
            }
            Token t;
            t.type = TokenType::Identifier;
            t.value = quotedVal;
            t.location = {line, startCol, static_cast<int>(quotedVal.length()) + 2};
            tokens.append(t);
            continue;
        }
        
        // 7. 识别普通标识符与中文字符
        if (c.isLetterOrNumber() || c == '_' || c.unicode() >= 0x4E00) {
            int startCol = col;
            QString val;
            while (i < len && (m_source[i].isLetterOrNumber() || m_source[i] == '_' || m_source[i].unicode() >= 0x4E00)) {
                val.append(m_source[i]);
                i++;
                col++;
            }
            
            Token t;
            t.location = {line, startCol, static_cast<int>(val.length())};
            t.value = val;
            if (val == "participant") {
                t.type = TokenType::Participant;
            } else if (val == "class" || val == "interface" || val == "enum") {
                t.type = TokenType::Class;
            } else {
                t.type = TokenType::Identifier;
            }
            tokens.append(t);
            continue;
        }
        
        // 8. 其它未知符号
        Token t;
        t.type = TokenType::Unknown;
        t.value = QString(c);
        t.location = {line, col, 1};
        tokens.append(t);
        i++;
        col++;
    }
    
    // 放入文件终结符
    Token tEof;
    tEof.type = TokenType::Eof;
    tEof.location = {line, col, 0};
    tokens.append(tEof);
    
    return tokens;
}

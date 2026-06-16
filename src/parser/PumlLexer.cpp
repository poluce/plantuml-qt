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
            t.location = {line, startCol, val.length()};
            t.value = val;
            if (val == "@startuml") {
                t.type = TokenType::StartUml;
            } else if (val == "@enduml") {
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
            tStr.location = {line, startCol, qMax(1, strVal.length())};
            tokens.append(tStr);
            continue;
        }
        
        // 5. 识别箭头
        if (c == '-') {
            int startCol = col;
            if (i + 2 < len && m_source[i + 1] == '-' && m_source[i + 2] == '>') {
                Token t;
                t.type = TokenType::DottedArrow;
                t.location = {line, startCol, 3};
                tokens.append(t);
                i += 3;
                col += 3;
                continue;
            } else if (i + 1 < len && m_source[i + 1] == '>') {
                Token t;
                t.type = TokenType::Arrow;
                t.location = {line, startCol, 2};
                tokens.append(t);
                i += 2;
                col += 2;
                continue;
            }
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
            t.location = {line, startCol, quotedVal.length() + 2};
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
            t.location = {line, startCol, val.length()};
            t.value = val;
            if (val == "participant") {
                t.type = TokenType::Participant;
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

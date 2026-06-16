#include "PumlParser.h"

PumlParser::PumlParser(const QVector<Token> &tokens)
    : m_tokens(tokens)
{
}

Token PumlParser::peek() const
{
    if (m_pos < m_tokens.size()) {
        return m_tokens[m_pos];
    }
    // 到达末尾返回 Eof Token
    return Token{TokenType::Eof, "", SourceLocation()};
}

Token PumlParser::next()
{
    Token t = peek();
    if (m_pos < m_tokens.size()) {
        m_pos++;
    }
    return t;
}

bool PumlParser::isEof() const
{
    return peek().type == TokenType::Eof;
}

void PumlParser::skipNewlines()
{
    while (!isEof() && peek().type == TokenType::Newline) {
        next();
    }
}

void PumlParser::ensureParticipantExists(SequenceDiagramAst *ast, const QString &id, const SourceLocation &loc)
{
    for (const auto &p : ast->participants) {
        if (p.id == id) {
            return;
        }
    }
    ParticipantDecl decl;
    decl.id = id;
    decl.displayName = id; // 隐式自动补齐的节点，显示名等于其ID
    decl.location = loc;
    ast->participants.append(decl);
}

ParseResult PumlParser::parse()
{
    ParseResult result;
    result.ast = std::make_unique<SequenceDiagramAst>();
    
    skipNewlines();
    
    // 1. 语法头部校验：可选的 @startuml (若没有则报警告继续)
    if (peek().type == TokenType::StartUml) {
        next();
    } else {
        ParseError err;
        err.location = peek().location;
        err.message = "第一行建议填写 '@startuml' 声明。";
        err.severity = ParseErrorSeverity::Warning;
        result.errors.append(err);
    }
    
    while (!isEof()) {
        skipNewlines();
        if (isEof()) break;
        
        Token t = peek();
        
        // 解析遇到 @enduml 时结束
        if (t.type == TokenType::EndUml) {
            next();
            break;
        }
        
        // 2. 解析显式参与者声明: participant ID (as "Display Name")?
        if (t.type == TokenType::Participant) {
            next(); // 吃掉 participant
            
            Token idToken = peek();
            if (idToken.type != TokenType::Identifier) {
                ParseError err;
                err.location = idToken.location;
                err.message = "participant 关键字后需要紧跟参与者标识符。";
                result.errors.append(err);
                
                // 错误恢复：吃掉这一整行，直到换行
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next(); // 吃掉标识符
            
            QString id = idToken.value;
            QString displayName = id;
            
            // 处理别名子语法: participant "Display Name" as alias
            // 此时 idToken 代表 "Display Name"，下一个 "as" 后面的 aliasToken 代表真正的 ID
            if (peek().type == TokenType::Identifier && peek().value == "as") {
                next(); // 吃掉 "as"
                Token aliasToken = peek();
                if (aliasToken.type != TokenType::Identifier) {
                    ParseError err;
                    err.location = aliasToken.location;
                    err.message = "as 关键字后需要紧跟别名标识符。";
                    result.errors.append(err);
                    while (!isEof() && peek().type != TokenType::Newline) next();
                    continue;
                }
                next(); // 吃掉别名
                
                // 语义模型重组，确保 alias 充当核心索引
                id = aliasToken.value;
                displayName = idToken.value;
            }
            
            ParticipantDecl decl;
            decl.id = id;
            decl.displayName = displayName;
            decl.location = idToken.location;
            result.ast->participants.append(decl);
            
            // 行尾多余字符容错
            if (peek().type != TokenType::Newline && peek().type != TokenType::Eof) {
                ParseError err;
                err.location = peek().location;
                err.message = "行尾存在多余的无效字符。";
                result.errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
            }
            continue;
        }
        
        // 3. 解析消息关系行: Source -> Target : Message
        if (t.type == TokenType::Identifier) {
            Token fromToken = next(); // 吃掉 from 节点
            
            Token arrowToken = peek();
            if (arrowToken.type != TokenType::Arrow && arrowToken.type != TokenType::DottedArrow) {
                ParseError err;
                err.location = arrowToken.location;
                err.message = "期望的消息连接符号为 '->' 或 '-->'。";
                result.errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next(); // 吃掉连线
            
            Token toToken = peek();
            if (toToken.type != TokenType::Identifier) {
                ParseError err;
                err.location = toToken.location;
                err.message = "消息连接符后缺少目标参与者标识符。";
                result.errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next(); // 吃掉 to 节点
            
            Token colonToken = peek();
            if (colonToken.type != TokenType::Colon) {
                ParseError err;
                err.location = colonToken.location;
                err.message = "连线表达式后必须带有冒号 ':' 和消息内容描述。";
                result.errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next(); // 吃掉冒号
            
            Token msgToken = peek();
            QString msgText;
            if (msgToken.type == TokenType::String) {
                msgText = msgToken.value;
                next(); // 吃掉消息内容
            }
            
            // 在语义表中隐式补全这两个参与者
            ensureParticipantExists(result.ast.get(), fromToken.value, fromToken.location);
            ensureParticipantExists(result.ast.get(), toToken.value, toToken.location);
            
            MessageStatement stmt;
            stmt.from = fromToken.value;
            stmt.to = toToken.value;
            stmt.text = msgText;
            stmt.kind = (arrowToken.type == TokenType::Arrow) ? MessageKind::Sync : MessageKind::Reply;
            stmt.location = arrowToken.location;
            result.ast->statements.append(stmt);
            continue;
        }
        
        // 4. 不识别的行，记录错误并跳过
        ParseError err;
        err.location = t.location;
        err.message = QString("无法识别的 PlantUML 语法结构: '%1'。").arg(t.value);
        result.errors.append(err);
        while (!isEof() && peek().type != TokenType::Newline) next();
    }
    
    return result;
}

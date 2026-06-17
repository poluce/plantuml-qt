#include "PumlParser.h"
#include <QDebug>

PumlParser::PumlParser(const QVector<Token> &tokens)
    : m_tokens(tokens)
{
}

Token PumlParser::peek() const
{
    if (m_pos < m_tokens.size()) {
        return m_tokens[m_pos];
    }
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

ParseResult PumlParser::parse()
{
    ParseResult result;
    
    // 1. 前瞻扫描，判定图类型
    bool isClass = false;
    for (const auto &t : m_tokens) {
        if (t.type == TokenType::Class || t.type == TokenType::Inherit) {
            isClass = true;
            break;
        }
    }
    
    // 2. 根据类型自动执行分流解析，返回多态 AST
    if (isClass) {
        result.ast = parseClassDiagram(result.errors);
    } else {
        result.ast = parseSequenceDiagram(result.errors);
    }
    
    return result;
}

// ================= 时序图解析逻辑 =================
std::unique_ptr<SequenceDiagramAst> PumlParser::parseSequenceDiagram(QVector<ParseError> &errors)
{
    auto ast = std::make_unique<SequenceDiagramAst>();
    
    skipNewlines();
    
    if (peek().type == TokenType::StartUml) {
        next();
        while (!isEof() && peek().type != TokenType::Newline) {
            next();
        }
    } else {
        ParseError err;
        err.location = peek().location;
        err.message = "第一行建议填写 '@startuml' 声明。";
        err.severity = ParseErrorSeverity::Warning;
        errors.append(err);
    }
    
    while (!isEof()) {
        skipNewlines();
        if (isEof()) break;
        
        Token t = peek();
        
        if (t.type == TokenType::EndUml) {
            next();
            break;
        }
        
        if (t.type == TokenType::Participant) {
            next();
            
            Token idToken = peek();
            if (idToken.type != TokenType::Identifier) {
                ParseError err;
                err.location = idToken.location;
                err.message = "participant 关键字后需要紧跟参与者标识符。";
                errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next();
            
            QString id = idToken.value;
            QString displayName = id;
            
            if (peek().type == TokenType::Identifier && peek().value == "as") {
                next();
                Token aliasToken = peek();
                if (aliasToken.type != TokenType::Identifier) {
                    ParseError err;
                    err.location = aliasToken.location;
                    err.message = "as 关键字后需要紧跟别名标识符。";
                    errors.append(err);
                    while (!isEof() && peek().type != TokenType::Newline) next();
                    continue;
                }
                next();
                
                id = aliasToken.value;
                displayName = idToken.value;
            }
            
            ParticipantDecl decl;
            decl.id = id;
            decl.displayName = displayName;
            decl.location = idToken.location;
            ast->participants.append(decl);
            
            if (peek().type != TokenType::Newline && peek().type != TokenType::Eof) {
                ParseError err;
                err.location = peek().location;
                err.message = "行尾存在多余的无效字符。";
                errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
            }
            continue;
        }
        
        if (t.type == TokenType::Identifier) {
            Token fromToken = next();
            
            Token arrowToken = peek();
            if (arrowToken.type != TokenType::Arrow && arrowToken.type != TokenType::DottedArrow) {
                ParseError err;
                err.location = arrowToken.location;
                err.message = "期望的消息连接符号为 '->' 或 '-->'。";
                errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next();
            
            Token toToken = peek();
            if (toToken.type != TokenType::Identifier) {
                ParseError err;
                err.location = toToken.location;
                err.message = "消息连接符后缺少目标参与者标识符。";
                errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next();
            
            Token colonToken = peek();
            if (colonToken.type != TokenType::Colon) {
                ParseError err;
                err.location = colonToken.location;
                err.message = "连线表达式后必须带有冒号 ':' 和消息内容描述。";
                errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next();
            
            Token msgToken = peek();
            QString msgText;
            if (msgToken.type == TokenType::String) {
                msgText = msgToken.value;
                next();
            }
            
            ensureParticipantExists(ast.get(), fromToken.value, fromToken.location);
            ensureParticipantExists(ast.get(), toToken.value, toToken.location);
            
            MessageStatement stmt;
            stmt.from = fromToken.value;
            stmt.to = toToken.value;
            stmt.text = msgText;
            stmt.kind = (arrowToken.type == TokenType::Arrow) ? MessageKind::Sync : MessageKind::Reply;
            stmt.location = arrowToken.location;
            ast->statements.append(stmt);
            continue;
        }
        
        ParseError err;
        err.location = t.location;
        err.message = QString("无法识别的 PlantUML 时序图语法: '%1'。").arg(t.value);
        errors.append(err);
        while (!isEof() && peek().type != TokenType::Newline) next();
    }
    
    return ast;
}

void PumlParser::ensureParticipantExists(SequenceDiagramAst *ast, const QString &id, const SourceLocation &loc)
{
    for (const auto &p : ast->participants) {
        if (p.id == id) return;
    }
    ParticipantDecl decl;
    decl.id = id;
    decl.displayName = id;
    decl.location = loc;
    ast->participants.append(decl);
}

// ================= 类图解析逻辑 =================
std::unique_ptr<ClassDiagramAst> PumlParser::parseClassDiagram(QVector<ParseError> &errors)
{
    auto ast = std::make_unique<ClassDiagramAst>();
    int packageDepth = 0;
    QString currentPackageId = "";
    
    skipNewlines();
    
    if (peek().type == TokenType::StartUml) {
        next();
        while (!isEof() && peek().type != TokenType::Newline) {
            next();
        }
    }
    
    while (!isEof()) {
        skipNewlines();
        if (isEof()) break;
        
        Token t = peek();
        
        if (t.type == TokenType::EndUml) {
            next();
            break;
        }
        
        // 跳过 skinparam 样式块以及 left/right layout 指令
        if (t.type == TokenType::Identifier && (t.value == "skinparam" || t.value == "left" || t.value == "right")) {
            QString cmd = t.value;
            next();
            
            if (cmd == "skinparam") {
                // 判断是否跟了多行大括号配置块，如 skinparam class { ... }
                while (!isEof() && peek().type != TokenType::LeftBrace && peek().type != TokenType::Newline) {
                    next();
                }
                if (peek().type == TokenType::LeftBrace) {
                    next(); // 吃掉左大括号 {
                    int braceDepth = 1;
                    while (!isEof() && braceDepth > 0) {
                        Token bt = next();
                        if (bt.type == TokenType::LeftBrace) braceDepth++;
                        else if (bt.type == TokenType::RightBrace) braceDepth--;
                    }
                } else {
                    // 单行 skinparam，吃掉整行
                    while (!isEof() && peek().type != TokenType::Newline) next();
                }
            } else {
                // 判断是否是 left to right direction
                if (cmd == "left") {
                    // 查看后两个 Token，是否为 to 和 right
                    Token t2 = peek();
                    if (t2.type == TokenType::Identifier && t2.value == "to") {
                        ast->leftToRight = true;
                    }
                }
                // left to right direction 等布局指令，吃掉整行
                while (!isEof() && peek().type != TokenType::Newline) next();
            }
            continue;
        }
        
        // 过滤 package 块声明，收集包ID与颜色
        if (t.type == TokenType::Identifier && t.value == "package") {
            next(); // 吃掉 package
            QString pkgId = "";
            QString pkgColor = "";
            if (peek().type == TokenType::Identifier || peek().type == TokenType::String) {
                pkgId = next().value; // 捕获包名 (Domain Layer 等)
            }
            if (peek().type == TokenType::Unknown && peek().value == "#") {
                next(); // 吃掉 #
                if (peek().type == TokenType::Identifier) {
                    pkgColor = "#" + next().value; // 捕获具体颜色码 (e.g. fff7ed)
                }
            }
            skipNewlines();
            if (peek().type == TokenType::LeftBrace) {
                next(); // 吃掉左括号 {
                packageDepth++;
                currentPackageId = pkgId;
                
                ClassDiagramAst::PackageDecl pkg;
                pkg.id = pkgId;
                pkg.displayName = pkgId;
                pkg.color = pkgColor;
                ast->packages.append(pkg);
            }
            continue;
        }
        
        // 在外部顶层匹配并吃掉 package 的闭合大括号 }
        if (t.type == TokenType::RightBrace && packageDepth > 0) {
            next();
            packageDepth--;
            if (packageDepth == 0) {
                currentPackageId = "";
            }
            continue;
        }
        
        // 1. 解析 class/interface/enum 声明: class Name { members }
        if (t.type == TokenType::Class) {
            QString meta = t.value; // "class" / "interface" / "enum"
            next(); // 吃掉关键字
            
            Token idToken = peek();
            if (idToken.type != TokenType::Identifier) {
                ParseError err;
                err.location = idToken.location;
                err.message = QString("%1 关键字后缺少名称标识符。").arg(meta);
                errors.append(err);
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next(); // 吃名字
            
            QString classId = idToken.value;
            QVector<QString> members;
            
            skipNewlines();
            
            // 优雅地忽略类名之后、大括号之前的 stereotype (<<...>>) 和颜色定义
            while (!isEof() && peek().type != TokenType::LeftBrace && peek().type != TokenType::Newline) {
                next(); // 吞掉修饰词
            }
            
            skipNewlines();
            
            // 解析大括号内的成员
            if (peek().type == TokenType::LeftBrace) {
                next(); // 吃掉 {
                
                while (!isEof()) {
                    skipNewlines();
                    if (isEof()) break;
                    
                    if (peek().type == TokenType::RightBrace) {
                        next(); // 吃掉 }
                        break;
                    }
                    
                    QString memberLine;
                    SourceLocation memberLoc = peek().location;
                    Q_UNUSED(memberLoc);
                    
                    // 抓取这一行直到 Newline
                    while (!isEof() && peek().type != TokenType::Newline) {
                        Token mt = next();
                        QString piece = mt.value;
                        
                        // 对于没有普通 value 的特殊符号做反向恢复拼接
                        if (piece.isEmpty()) {
                            if (mt.type == TokenType::Colon) piece = ":";
                            else if (mt.type == TokenType::Arrow) piece = "->";
                            else if (mt.type == TokenType::DottedArrow) piece = "-->";
                            else if (mt.type == TokenType::Inherit) piece = "<|--";
                            else if (mt.type == TokenType::Composition) piece = "*--";
                            else if (mt.type == TokenType::Aggregation) piece = "o--";
                            else if (mt.type == TokenType::Realization) piece = "<|..";
                            else if (mt.type == TokenType::Dependency) piece = "..>";
                            else if (mt.type == TokenType::InheritRight) piece = "--|>";
                            else if (mt.type == TokenType::RealizeRight) piece = "..|>";
                            else if (mt.type == TokenType::LeftBrace) piece = "{";
                            else if (mt.type == TokenType::RightBrace) piece = "}";
                        }
                        
                        if (!memberLine.isEmpty() && !piece.startsWith('(') && !piece.startsWith(')')) {
                            memberLine.append(" ");
                        }
                        memberLine.append(piece);
                    }
                    
                    if (!memberLine.trimmed().isEmpty()) {
                        members.append(memberLine.trimmed());
                    }
                }
            }
            
            // 查重：若已经通过连线隐式声明过，则为其装载多行成员，否则新建
            bool found = false;
            for (auto &c : ast->classes) {
                if (c.id == classId) {
                    c.members = members;
                    c.metaType = meta;
                    c.packageName = currentPackageId;
                    c.location = idToken.location;
                    found = true;
                    qDebug() << "[Parser] 更新已存在类声明:" << classId << "行号:" << c.location.line << "包:" << currentPackageId;
                    break;
                }
            }
            if (!found) {
                ClassDecl decl;
                decl.id = classId;
                decl.metaType = meta;
                decl.packageName = currentPackageId;
                decl.members = members;
                decl.location = idToken.location;
                ast->classes.append(decl);
                qDebug() << "[Parser] 显式解析到类声明:" << classId << "行号:" << decl.location.line << "包:" << currentPackageId;
            }
            continue;
        }
        
        // 2. 解析类关系: ClassA <|-- ClassB : text 或 ClassA --> ClassB 等多种关系
        if (t.type == TokenType::Identifier) {
            Token fromToken = next();
            
            Token relToken = peek();
            if (relToken.type != TokenType::Arrow && 
                relToken.type != TokenType::DottedArrow && 
                relToken.type != TokenType::Inherit &&
                relToken.type != TokenType::Composition &&
                relToken.type != TokenType::Aggregation &&
                relToken.type != TokenType::Realization &&
                relToken.type != TokenType::Dependency &&
                relToken.type != TokenType::InheritRight &&
                relToken.type != TokenType::RealizeRight) {
                ParseError err;
                err.location = relToken.location;
                err.message = "期望的类关系连接符为 '<|--'、'<|..'、'*--'、'o--'、'..>'、'--|>'、'..|>' 或 '-->'/'->'。";
                errors.append(err);
                qDebug() << "[Parser] 关系连接符错误于第" << err.location.line << "行:" << err.message;
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next(); // 吃连线
            
            Token toToken = peek();
            if (toToken.type != TokenType::Identifier) {
                ParseError err;
                err.location = toToken.location;
                err.message = "关系连接符后缺少目标类名标识符。";
                errors.append(err);
                qDebug() << "[Parser] 缺少目标类名错误于第" << err.location.line << "行:" << err.message;
                while (!isEof() && peek().type != TokenType::Newline) next();
                continue;
            }
            next(); // 吃 to
            
            // 提取连线说明标签
            QString labelText;
            if (peek().type == TokenType::Colon) {
                next(); // 吃冒号
                if (peek().type == TokenType::String) {
                    labelText = next().value;
                }
            }
            
            // 隐式补全两端的类声明
            ensureClassExists(ast.get(), fromToken.value, fromToken.location);
            ensureClassExists(ast.get(), toToken.value, toToken.location);
            
            RelationDecl rel;
            rel.from = fromToken.value;
            rel.to = toToken.value;
            rel.text = labelText;
            
            if (relToken.type == TokenType::Inherit) {
                rel.kind = RelationKind::Inheritance;
            } else if (relToken.type == TokenType::Composition) {
                rel.kind = RelationKind::Composition;
            } else if (relToken.type == TokenType::Aggregation) {
                rel.kind = RelationKind::Aggregation;
            } else if (relToken.type == TokenType::Realization) {
                rel.kind = RelationKind::Realization;
            } else if (relToken.type == TokenType::Dependency) {
                rel.kind = RelationKind::Dependency;
            } else if (relToken.type == TokenType::InheritRight) {
                rel.from = toToken.value; // 右向继承：对调归一化
                rel.to = fromToken.value;
                rel.kind = RelationKind::Inheritance;
            } else if (relToken.type == TokenType::RealizeRight) {
                rel.from = toToken.value; // 右向实现：对调归一化
                rel.to = fromToken.value;
                rel.kind = RelationKind::Realization;
            } else {
                rel.kind = RelationKind::Association;
            }
            
            rel.location = relToken.location;
            ast->relations.append(rel);
            continue;
        }
        
        ParseError err;
        err.location = t.location;
        err.message = QString("无法识别的 PlantUML 类图语法: '%1'。").arg(t.value);
        errors.append(err);
        qDebug() << "[Parser] 无法识别语法错误于第" << err.location.line << "行:" << err.message;
        while (!isEof() && peek().type != TokenType::Newline) next();
    }
    
    return ast;
}

void PumlParser::ensureClassExists(ClassDiagramAst *ast, const QString &id, const SourceLocation &loc)
{
    for (const auto &c : ast->classes) {
        if (c.id == id) return;
    }
    ClassDecl decl;
    decl.id = id;
    decl.location = loc;
    ast->classes.append(decl);
    qDebug() << "[Parser] 关系中隐式创建类:" << id << "首次行号:" << loc.line;
}

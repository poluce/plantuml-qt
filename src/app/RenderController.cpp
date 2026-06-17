#include "RenderController.h"
#include "../parser/PumlLexer.h"
#include "../parser/PumlParser.h"
#include "../ast/DiagramAst.h"

RenderController::RenderController(DiagramScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
{
    // 配置 300ms 延迟的输入防抖定时器
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);
    connect(&m_debounceTimer, &QTimer::timeout, this, &RenderController::doRender);
}

void RenderController::setSourceText(const QString &text)
{
    m_sourceText = text;
    m_debounceTimer.start(); // 打字时不断重启计时器以执行防抖
}

void RenderController::doRender()
{
    if (!m_scene) return;
    
    // 1. 词法扫描分词
    PumlLexer lexer(m_sourceText);
    QVector<Token> tokens = lexer.tokenize();
    
    // 2. 语法解析生成 AST 并收集错误
    PumlParser parser(tokens);
    ParseResult parseResult = parser.parse();
    
    // 3. 执行布局与图元生成 (容错设计：即使有非致命错，也尽量生成图形显示)
    if (parseResult.ast) {
        RenderDocument doc = m_layoutEngine.layout(*parseResult.ast, m_theme);
        m_renderer.render(m_scene, doc, m_theme);
    } else {
        m_scene->clearDiagram();
    }
    
    // 4. 将语法错误向上反馈给 MainWindow 界面展示
    emit renderFinished(parseResult.errors);
}

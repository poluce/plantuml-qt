#include "RenderController.h"
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

void RenderController::renderDiagramIndex(int index)
{
    if (!m_scene) return;
    if (index >= 0 && index < m_renderedDocs.size()) {
        m_renderer.render(m_scene, m_renderedDocs[index], m_theme);
    } else {
        m_scene->clearDiagram();
    }
}

void RenderController::doRender()
{
    if (!m_scene) return;
    
    // 1. 进行多图分块切分
    QVector<DiagramBlock> blocks = splitDiagrams(m_sourceText);
    m_renderedDocs.clear();
    
    QStringList titles;
    QVector<ParseError> allErrors;
    
    if (blocks.isEmpty()) {
        // 如果切分结果为空，则清空场景并通知
        m_scene->clearDiagram();
        emit renderFinished(titles, allErrors);
        return;
    }
    
    // 2. 对每个 block 进行独立解析与排版布局生成
    for (const auto &block : blocks) {
        titles.append(block.title);
        
        PumlParser parser(block.content);
        ParseResult result = parser.parse();
        
        // 纠正错误行号
        for (auto &err : result.errors) {
            err.location.line += block.startLine - 1;
            allErrors.append(err);
        }
        
        RenderDocument doc;
        if (result.ast) {
            doc = m_layoutEngine.layout(*result.ast, m_theme);
        }
        m_renderedDocs.append(doc);
    }
    
    // 3. 默认渲染第 0 个图
    renderDiagramIndex(0);
    
    // 4. 将图的标题列表与语法错误向上反馈给 MainWindow 界面展示
    emit renderFinished(titles, allErrors);
}

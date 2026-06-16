#include "mainwindow.h"
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextCursor>
#include <QTextBlock>
#include <QStyle>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 1. 初始化图形场景与自定义视图
    graphicsScene = new DiagramScene(this);
    graphicsView = new DiagramView(this);
    graphicsView->setScene(graphicsScene);
    
    // 2. 初始化本地编译流水线控制器
    renderController = new RenderController(graphicsScene, this);
    
    // 3. 构建布局界面与 QSS 浅色主题
    setupUi();
    setupStyles();
    
    // 4. 连接控制层与视图层的信号槽
    connect(editor, &QPlainTextEdit::textChanged, this, &MainWindow::onTextChanged);
    connect(renderController, &RenderController::renderFinished, this, &MainWindow::onRenderFinished);
    
    // 拦截右侧点击，自动选中并跳转到左侧对应行
    connect(graphicsScene, &DiagramScene::itemActivated, this, &MainWindow::onItemActivated);
    
    // 5. 填入初始 PlantUML 本地时序图案例 (完全使用中文标签和注释，测试中文字体与本地解析)
    editor->setPlainText(
        "@startuml\n"
        "skinparam backgroundColor #ffffff\n"
        "skinparam usecaseBackgroundColor #e0e7ff\n"
        "skinparam usecaseBorderColor #6366f1\n"
        "skinparam usecaseArrowColor #4f46e5\n"
        "skinparam actorBorderColor #4f46e5\n"
        "skinparam actorBackgroundColor #e0e7ff\n"
        "skinparam objectBorderColor #4f46e5\n"
        "skinparam objectBackgroundColor #f3f4f6\n"
        "skinparam classBorderColor #4f46e5\n"
        "skinparam arrowColor #4f46e5\n"
        "skinparam classHeaderBackgroundColor #e0e7ff\n"
        "skinparam classBackgroundColor #ffffff\n"
        "skinparam classAttributeFontColor #18181b\n"
        "skinparam classAttributeFontSize 12\n"
        "skinparam classFontColor #18181b\n"
        "skinparam classFontSize 14\n"
        "skinparam noteBackgroundColor #fef08a\n"
        "skinparam noteBorderColor #ca8a04\n"
        "skinparam noteFontColor #713f12\n\n"
        "header PlantUML 本地矢量查看器\n"
        "footer 第 1 页，共 1 页\n\n"
        "participant \"文本编辑器\" as edit\n"
        "participant \"控制器\" as ctrl\n"
        "participant \"解析引擎\" as parser\n"
        "participant \"布局引擎\" as layout\n"
        "participant \"场景渲染\" as renderer\n\n"
        "edit -> ctrl : 输入变化 (防抖响应)\n"
        "ctrl -> parser : 启动词法与语法分析\n"
        "parser -> layout : 提取 AST 树并执行几何计算\n"
        "layout -> renderer : 输出 RenderDoc 并装载图元\n"
        "renderer --> edit : 渲染完毕 (就绪并双向跳转)\n"
        "note left of edit : 双击右侧图元\n可以直接高亮编辑器源码行！\n"
        "@enduml"
    );
    
    // 首次主动触发一次本地解析
    renderController->setSourceText(editor->toPlainText());
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("PlantUML 矢量查看器 (纯本地编译版)"));
    resize(1200, 800);
    
    // 1. 创建顶部工具栏
    QToolBar *toolBar = addToolBar(tr("主工具栏"));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    
    QLabel *titleLabel = new QLabel(tr("PlantUML 查看器"), this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 11pt; color: #18181b;");
    toolBar->addWidget(titleLabel);
    
    toolBar->addSeparator();
    
    // 状态提示区
    statusLabel = new QLabel(tr("就绪"), this);
    statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");
    toolBar->addWidget(statusLabel);
    
    // 弹簧占位，让按钮右对齐
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);
    
    // 自适应大小按钮
    btnFit = new QPushButton(tr("适应大小"), this);
    connect(btnFit, &QPushButton::clicked, this, &MainWindow::fitView);
    toolBar->addWidget(btnFit);
    
    // 1:1 复位缩放按钮
    btnReset = new QPushButton(tr("复位缩放"), this);
    connect(btnReset, &QPushButton::clicked, this, &MainWindow::resetView);
    toolBar->addWidget(btnReset);
    
    // 强制解析按钮
    btnRefresh = new QPushButton(tr("强制解析"), this);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::onTextChanged);
    toolBar->addWidget(btnRefresh);
    
    // 2. 双栏 Splitter 容器
    splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter);
    
    // 左栏：编辑器
    editor = new QPlainTextEdit(this);
    editor->setPlaceholderText(tr("在此输入 PlantUML 代码..."));
    splitter->addWidget(editor);
    
    // 右栏：视口
    splitter->addWidget(graphicsView);
    
    QList<int> sizes;
    sizes << 500 << 700;
    splitter->setSizes(sizes);
}

void MainWindow::setupStyles()
{
    // 现代浅色极简设计 QSS
    QString qss = R"(
        QMainWindow {
            background-color: #f9fafb;
        }
        QSplitter::handle {
            background-color: #e5e7eb;
            width: 4px;
        }
        QPlainTextEdit {
            background-color: #ffffff;
            color: #18181b;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            font-family: 'Consolas', 'Fira Code', 'Courier New', monospace;
            font-size: 13px;
            line-height: 1.5;
            padding: 12px;
        }
        QPlainTextEdit:focus {
            border: 1px solid #6366f1;
        }
        QGraphicsView {
            background-color: #f3f4f6;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
        }
        QToolBar {
            background-color: #ffffff;
            border-bottom: 1px solid #e5e7eb;
            padding: 8px 16px;
            spacing: 16px;
        }
        QPushButton {
            background-color: #f4f4f5;
            color: #27272a;
            border: 1px solid #e4e4e7;
            border-radius: 6px;
            padding: 6px 16px;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #e4e4e7;
            border-color: #6366f1;
        }
        QPushButton:pressed {
            background-color: #d4d4d8;
            border-color: #818cf8;
        }
        QLabel {
            color: #71717a;
        }
    )";
    setStyleSheet(qss);
}

void MainWindow::onTextChanged()
{
    // 输入改变，向控制器发出文本
    renderController->setSourceText(editor->toPlainText());
}

void MainWindow::onRenderFinished(const QVector<ParseError> &errors)
{
    if (errors.isEmpty()) {
        statusLabel->setText(tr("就绪"));
        statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");
    } else {
        // 抓取第一条语法错误直接在状态栏中以红字警示
        const auto &err = errors.first();
        statusLabel->setText(QString(tr("语法错误：第 %1 行 - %2")).arg(err.location.line).arg(err.message));
        statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
    }
}

void MainWindow::onItemActivated(QString semanticId, SourceLocation location)
{
    Q_UNUSED(semanticId);
    
    if (location.line <= 0) return;
    
    // 获取指定行数的文本块 (行号从 0 开始)
    QTextBlock block = editor->document()->findBlockByLineNumber(location.line - 1);
    if (block.isValid()) {
        QTextCursor cursor(block);
        
        // 选中这一整行，反白显示，增加视觉提示
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
        
        // 强制编辑区获取焦点
        editor->setFocus();
    }
}

void MainWindow::resetView()
{
    graphicsView->resetZoom();
}

void MainWindow::fitView()
{
    graphicsView->fitToContent();
}

#include "mainwindow.h"
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextCursor>
#include <QTextBlock>
#include <QStyle>
#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentListItem(nullptr)
{
    // 1. 初始化核心渲染三件套
    graphicsScene = new DiagramScene(this);
    graphicsView = new DiagramView(this);
    graphicsView->setScene(graphicsScene);
    
    renderController = new RenderController(graphicsScene, this);
    
    // 2. 组装三栏 UI 界面并设置 QSS 皮肤
    setupUi();
    setupStyles();
    
    // 3. 关联控件的信号槽
    connect(editor, &QPlainTextEdit::textChanged, this, &MainWindow::onTextChanged);
    connect(renderController, &RenderController::renderFinished, this, &MainWindow::onRenderFinished);
    connect(graphicsScene, &DiagramScene::itemActivated, this, &MainWindow::onItemActivated);
    
    // 关键多项目联动：关联列表项的电流更改信号
    connect(fileList, &QListWidget::currentItemChanged, this, &MainWindow::onCurrentFileChanged);
    
    // 4. 新建一个默认的未命名首屏案例，优化冷启动体验
    QListWidgetItem *defaultItem = new QListWidgetItem(tr("untitled.puml"), fileList);
    OpenedFile defaultFile;
    defaultFile.filePath = ""; // 空代表内存临时草稿
    defaultFile.content = 
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
        "header PlantUML 矢量查看器\n"
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
        "@enduml";
    
    // 将默认文件数据载入关联映射表中
    m_files[defaultItem] = defaultFile;
    
    // 激活第一个默认文件 (这会自动触发 onCurrentFileChanged 进行首次解析绘制)
    fileList->setCurrentItem(defaultItem);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("PlantUML 矢量查看器 (纯本地多文件版)"));
    resize(1300, 800);
    
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
    
    // 弹簧占位，让按钮靠右对齐
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);
    
    // 适应大小按钮
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
    
    // 2. 核心三栏分割布局
    splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter);
    
    // 2.1 最左侧栏：项目树列表管理
    leftSidebar = new QWidget(this);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(leftSidebar);
    sidebarLayout->setContentsMargins(10, 10, 10, 10);
    sidebarLayout->setSpacing(8);
    
    // 打开本地文件按钮
    btnOpenFile = new QPushButton(tr("打开本地文件"), this);
    connect(btnOpenFile, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
    sidebarLayout->addWidget(btnOpenFile);
    
    // 已打开文件列表
    fileList = new QListWidget(this);
    sidebarLayout->addWidget(fileList);
    
    splitter->addWidget(leftSidebar);
    
    // 2.2 中间栏：源码编辑器
    editor = new QPlainTextEdit(this);
    editor->setPlaceholderText(tr("在此输入 PlantUML 代码..."));
    splitter->addWidget(editor);
    
    // 2.3 右侧栏：视口
    splitter->addWidget(graphicsView);
    
    // 设定初始分割比例：最左栏 200px，中栏 430px，右栏 670px
    QList<int> sizes;
    sizes << 200 << 430 << 670;
    splitter->setSizes(sizes);
}

void MainWindow::setupStyles()
{
    // 精美的浅色极简扁平设计 QSS (补充了 QListWidget 美化规则)
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
        QListWidget {
            background-color: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            color: #27272a;
            padding: 5px;
        }
        QListWidget::item {
            padding: 8px 12px;
            border-radius: 6px;
            margin-bottom: 2px;
        }
        QListWidget::item:hover {
            background-color: #f4f4f5;
        }
        QListWidget::item:selected {
            background-color: #e0e7ff;
            color: #6366f1;
            font-weight: bold;
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
    // 输入被修改，同步保存当前活跃项的内存草稿内容
    if (m_currentListItem && m_files.contains(m_currentListItem)) {
        m_files[m_currentListItem].content = editor->toPlainText();
    }
    
    // 驱动控制器重新局部计算渲染
    renderController->setSourceText(editor->toPlainText());
}

void MainWindow::onRenderFinished(const QVector<ParseError> &errors)
{
    if (errors.isEmpty()) {
        statusLabel->setText(tr("就绪"));
        statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");
    } else {
        const auto &err = errors.first();
        statusLabel->setText(QString(tr("语法错误：第 %1 行 - %2")).arg(err.location.line).arg(err.message));
        statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
    }
}

void MainWindow::onItemActivated(QString semanticId, SourceLocation location)
{
    Q_UNUSED(semanticId);
    
    if (location.line <= 0) return;
    
    QTextBlock block = editor->document()->findBlockByLineNumber(location.line - 1);
    if (block.isValid()) {
        QTextCursor cursor(block);
        
        // 选中这一整行，反白显示，增加视觉聚焦提示
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
        
        editor->setFocus();
    }
}

void MainWindow::onOpenFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("打开 PlantUML 文件"),
        QString(),
        tr("PlantUML 源码 (*.puml *.txt *.plantuml);;所有文件 (*.*)")
    );
    
    if (filePath.isEmpty()) return;
    
    // 1. 查重逻辑：若该绝对路径已被导入打开，直接聚焦跳转
    for (auto it = m_files.begin(); it != m_files.end(); ++it) {
        if (it.value().filePath == filePath) {
            fileList->setCurrentItem(it.key());
            return;
        }
    }
    
    // 2. 读取物理文件内容
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("读取错误"), tr("无法打开并读取此文件，请确认权限与文件有效性。"));
        return;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    // 3. 在 QListWidget 项目树中生成新 item
    QFileInfo fileInfo(filePath);
    QListWidgetItem *newItem = new QListWidgetItem(fileInfo.fileName(), fileList);
    
    OpenedFile openedFile;
    openedFile.filePath = filePath;
    openedFile.content = content;
    m_files[newItem] = openedFile;
    
    // 4. 激活新打开的文件 (会自动切换至该项目并绘制)
    fileList->setCurrentItem(newItem);
}

void MainWindow::onCurrentFileChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    // 1. 暂存即将切走的文件的最新编辑草稿 (确保内存同步)
    if (previous && m_files.contains(previous)) {
        m_files[previous].content = editor->toPlainText();
    }
    
    // 2. 载入切换过来的新文件内容
    if (current && m_files.contains(current)) {
        m_currentListItem = current;
        
        // 阻塞信号发射，防止大面积填充时的双重解析渲染消耗
        editor->blockSignals(true);
        editor->setPlainText(m_files[current].content);
        editor->blockSignals(false);
        
        // 刷新应用标题，如果是本地文件展示物理路径，否则展示未命名提示
        const auto &openedFile = m_files[current];
        if (openedFile.filePath.isEmpty()) {
            setWindowTitle(tr("PlantUML 矢量查看器 - [未命名临时草稿]"));
        } else {
            setWindowTitle(QString(tr("PlantUML 矢量查看器 - [%1]")).arg(openedFile.filePath));
        }
        
        // 3. 驱动流水线重新解析和高精矢量渲染
        renderController->setSourceText(openedFile.content);
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

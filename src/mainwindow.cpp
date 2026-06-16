#include "mainwindow.h"
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsSvgItem>
#include <QSvgRenderer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QByteArray>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 初始化后台组件
    encoder = new PlantUmlEncoder(this);
    networkManager = new QNetworkAccessManager(this);
    
    // 初始化 800ms 输入防抖定时器
    renderTimer = new QTimer(this);
    renderTimer->setSingleShot(true);
    renderTimer->setInterval(800);
    
    // 连接信号槽
    connect(renderTimer, &QTimer::timeout, this, &MainWindow::onRenderTimerTriggered);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onNetworkReplyFinished);
    
    // 设置 UI 和 QSS 皮肤
    setupUi();
    setupStyles();
    
    // 写入默认的高清晰度时序图与类图展示用例
    editor->setPlainText(
        "@startuml\n"
        "skinparam backgroundColor #1e1e24\n"
        "skinparam usecaseBackgroundColor #6366f1\n"
        "skinparam usecaseBorderColor #ef4444\n"
        "skinparam usecaseArrowColor #38bdf8\n"
        "skinparam actorBorderColor #818cf8\n"
        "skinparam actorBackgroundColor #818cf8\n"
        "skinparam objectBorderColor #38bdf8\n"
        "skinparam classBorderColor #6366f1\n"
        "skinparam arrowColor #e4e4e7\n"
        "skinparam classHeaderBackgroundColor #6366f1\n"
        "skinparam classBackgroundColor #27272a\n"
        "skinparam classAttributeFontColor #e4e4e7\n"
        "skinparam classAttributeFontSize 12\n"
        "skinparam classFontColor #e4e4e7\n"
        "skinparam classFontSize 14\n"
        "skinparam noteBackgroundColor #121214\n"
        "skinparam noteBorderColor #38bdf8\n"
        "skinparam noteFontColor #a1a1aa\n\n"
        "header PlantUML GraphicsScene Viewer\n"
        "footer Page 1 of 1\n\n"
        "class MainWindow {\n"
        "  - editor : QPlainTextEdit*\n"
        "  - graphicsView : PlantUmlView*\n"
        "  - graphicsScene : QGraphicsScene*\n"
        "  + onTextChanged()\n"
        "  + onRenderTimerTriggered()\n"
        "}\n\n"
        "class PlantUmlView {\n"
        "  + wheelEvent(QWheelEvent*)\n"
        "  + resetZoom()\n"
        "}\n\n"
        "MainWindow *-- PlantUmlView : Contains >\n"
        "note left of MainWindow : QGraphicsScene enables extremely\nsmooth vector scaling without pixelation!\n"
        "@enduml"
    );
    
    // 启动时立即进行首次渲染
    onRenderTimerTriggered();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("PlantUML GraphicsScene Viewer"));
    resize(1200, 800);
    
    // 1. 创建顶部工具栏
    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    
    // 工具栏左侧标题与状态
    QLabel *titleLabel = new QLabel(tr("PlantUML Viewer"), this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 11pt; color: #e4e4e7;");
    toolBar->addWidget(titleLabel);
    
    toolBar->addSeparator();
    
    statusLabel = new QLabel(tr("Ready"), this);
    statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");
    toolBar->addWidget(statusLabel);
    
    // 弹簧占位，让按钮靠右对齐
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);
    
    // 重置缩放按钮
    btnReset = new QPushButton(tr("Reset Zoom"), this);
    connect(btnReset, &QPushButton::clicked, this, &MainWindow::resetView);
    toolBar->addWidget(btnReset);
    
    // 手动刷新按钮
    btnRefresh = new QPushButton(tr("Force Refresh"), this);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::onRenderTimerTriggered);
    toolBar->addWidget(btnRefresh);
    
    // 2. 双栏 Splitter 布局
    splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter);
    
    // 左栏：代码编辑框
    editor = new QPlainTextEdit(this);
    editor->setPlaceholderText(tr("Enter your PlantUML code here..."));
    connect(editor, &QPlainTextEdit::textChanged, this, &MainWindow::onTextChanged);
    splitter->addWidget(editor);
    
    // 右栏：图形视口
    graphicsView = new PlantUmlView(this);
    graphicsScene = new QGraphicsScene(this);
    graphicsView->setScene(graphicsScene);
    splitter->addWidget(graphicsView);
    
    // 设定初始的左右分栏比例 (编辑器占45%，视口占55%)
    QList<int> sizes;
    sizes << 540 << 660;
    splitter->setSizes(sizes);
}

void MainWindow::setupStyles()
{
    // 精美的现代暗黑极客主题 QSS
    QString qss = R"(
        QMainWindow {
            background-color: #121214;
        }
        QSplitter::handle {
            background-color: #27272a;
            width: 4px;
        }
        QPlainTextEdit {
            background-color: #1e1e24;
            color: #e4e4e7;
            border: 1px solid #27272a;
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
            background-color: #121214;
            border: 1px solid #27272a;
            border-radius: 8px;
        }
        QToolBar {
            background-color: #1e1e24;
            border-bottom: 1px solid #27272a;
            padding: 8px 16px;
            spacing: 16px;
        }
        QPushButton {
            background-color: #27272a;
            color: #e4e4e7;
            border: 1px solid #3f3f46;
            border-radius: 6px;
            padding: 6px 16px;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #3f3f46;
            border-color: #6366f1;
        }
        QPushButton:pressed {
            background-color: #1e1e24;
            border-color: #818cf8;
        }
    )";
    setStyleSheet(qss);
}

void MainWindow::onTextChanged()
{
    // 打字过程中重置防抖计时器
    renderTimer->start();
}

void MainWindow::onRenderTimerTriggered()
{
    QString code = editor->toPlainText();
    if (code.trimmed().isEmpty()) {
        graphicsScene->clear();
        statusLabel->setText(tr("Empty"));
        statusLabel->setStyleSheet("color: #71717a; font-weight: bold;");
        return;
    }
    
    // C++ 调用 Deflate 压缩 + 自定义 Base64 编码
    QString encoded = encoder->encode(code);
    if (encoded.isEmpty()) return;
    
    statusLabel->setText(tr("Rendering..."));
    statusLabel->setStyleSheet("color: #38bdf8; font-weight: bold;");
    
    // 拼装 PlantUML 官方在线渲染服务器接口
    QUrl url("http://www.plantuml.com/plantuml/svg/" + encoded);
    QNetworkRequest request(url);
    
    // 发送异步网络请求
    networkManager->get(request);
}

void MainWindow::onNetworkReplyFinished(QNetworkReply *reply)
{
    // 处理重定向或网络错误
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray svgData = reply->readAll();
        
        // 验证下载到的字节流是否是合法的 XML/SVG 数据
        if (svgData.startsWith("<?xml") || svgData.contains("<svg")) {
            graphicsScene->clear();
            
            // 使用 QSvgRenderer 直接基于内存字节流进行渲染
            auto *svgItem = new QGraphicsSvgItem();
            auto *renderer = new QSvgRenderer(svgData, graphicsScene); // scene 拥有其生命周期
            
            svgItem->setSharedRenderer(renderer);
            graphicsScene->addItem(svgItem);
            
            // 设定场景的边界矩形为 SVG 的自然大小
            graphicsScene->setSceneRect(svgItem->boundingRect());
            
            // 重置缩放并把图置中
            graphicsView->resetZoom();
            
            statusLabel->setText(tr("Ready"));
            statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");
        } else {
            statusLabel->setText(tr("Diagram Syntax Error / Invalid SVG"));
            statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
        }
    } else {
        statusLabel->setText(tr("Network Error / Connection Failed"));
        statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
    }
    
    reply->deleteLater();
}

void MainWindow::resetView()
{
    graphicsView->resetZoom();
}

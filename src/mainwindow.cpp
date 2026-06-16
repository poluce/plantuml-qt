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
        "MainWindow *-- PlantUmlView : 包含 >\n"
        "note left of MainWindow : QGraphicsScene 能够实现\n极其平滑的矢量缩放，完全无锯齿！\n"
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
    setWindowTitle(tr("PlantUML 矢量查看器"));
    resize(1200, 800);
    
    // 1. 创建顶部工具栏
    QToolBar *toolBar = addToolBar(tr("主工具栏"));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    
    // 工具栏左侧标题与状态
    QLabel *titleLabel = new QLabel(tr("PlantUML 查看器"), this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 11pt; color: #18181b;");
    toolBar->addWidget(titleLabel);
    
    toolBar->addSeparator();
    
    statusLabel = new QLabel(tr("就绪"), this);
    statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");
    toolBar->addWidget(statusLabel);
    
    // 弹簧占位，让按钮靠右对齐
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);
    
    // 重置缩放按钮
    btnReset = new QPushButton(tr("复位缩放"), this);
    connect(btnReset, &QPushButton::clicked, this, &MainWindow::resetView);
    toolBar->addWidget(btnReset);
    
    // 手动刷新按钮
    btnRefresh = new QPushButton(tr("强制刷新"), this);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::onRenderTimerTriggered);
    toolBar->addWidget(btnRefresh);
    
    // 2. 双栏 Splitter 布局
    splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter);
    
    // 左栏：代码编辑框
    editor = new QPlainTextEdit(this);
    editor->setPlaceholderText(tr("在此输入 PlantUML 代码..."));
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
    // 精美的现代浅色（Modern Light Theme）极简设计
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
    // 打字过程中重置防抖计时器
    renderTimer->start();
}

void MainWindow::onRenderTimerTriggered()
{
    QString code = editor->toPlainText();
    if (code.trimmed().isEmpty()) {
        graphicsScene->clear();
        statusLabel->setText(tr("暂无内容"));
        statusLabel->setStyleSheet("color: #71717a; font-weight: bold;");
        return;
    }
    
    // C++ 调用 Deflate 压缩 + 自定义 Base64 编码
    QString encoded = encoder->encode(code);
    if (encoded.isEmpty()) return;
    
    statusLabel->setText(tr("正在渲染..."));
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
            
            statusLabel->setText(tr("就绪"));
            statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");
        } else {
            statusLabel->setText(tr("语法错误 / 无效的 SVG 图像"));
            statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
        }
    } else {
        statusLabel->setText(tr("网络错误 / 连接失败"));
        statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
    }
    
    reply->deleteLater();
}

void MainWindow::resetView()
{
    graphicsView->resetZoom();
}

#include "mainwindow.h"
#include "parser/PumlHighlighter.h"
#include <QThread>
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
#include <QScrollBar>
#include <QDebug>
#include <QRegularExpression>
#include <QMenu>
#include <QAction>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "graphics/items/ClassBoxItem.h"
#include "graphics/items/PackageGroupItem.h"
#include "graphics/items/RelationItem.h"
#include <QGraphicsDropShadowEffect>
#include <QEvent>

namespace {
    QTextBlock findSemanticBlock(QPlainTextEdit *editor, const QString &semanticId)
    {
        if (!editor || semanticId.isEmpty()) {
            return {};
        }

        const QString escapedId = QRegularExpression::escape(semanticId);
        const QRegularExpression re(
            QString(R"(^\s*(class|interface|enum|participant|actor)\s+"?%1"?(\s|\{|$))").arg(escapedId));

        for (QTextBlock block = editor->document()->firstBlock(); block.isValid(); block = block.next()) {
            if (re.match(block.text()).hasMatch()) {
                return block;
            }
        }
        return {};
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentListItem(nullptr)
{
    // 1. 初始化核心渲染三件套
    graphicsScene = new DiagramScene(this);
    graphicsView = new DiagramView(this);
    graphicsView->setScene(graphicsScene);
    graphicsView->installEventFilter(this); // 安装事件过滤器以实现自适应拉伸
    
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
    
    // 关联折叠/展开按钮的点击信号
    connect(btnToggleEditor, &QPushButton::clicked, this, [this]() {
        setEditorVisible(!m_editorVisible, true);
    });
    
    // 初始化外部文件监视器并绑定信号槽
    fileWatcher = new QFileSystemWatcher(this);
    connect(fileWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onFileChanged);
    

    
    // 启用列表项的自定义右键菜单并绑定信号槽
    fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(fileList, &QListWidget::customContextMenuRequested, this, &MainWindow::showFileListContextMenu);
    
    // 初始状态下隐藏 UML 编辑框
    setEditorVisible(false, false);
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
    statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    toolBar->addWidget(statusLabel);
    
    // 弹簧占位，让按钮靠右对齐
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    // 终端按钮 (添加在“适应大小”的左边)
    btnTerminal = new QPushButton(tr("终端"), this);
    connect(btnTerminal, &QPushButton::clicked, this, []() {
        qDebug() << "[MainWindow] 终端按钮被点击";
    });
    toolBar->addWidget(btnTerminal);
    
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
    
    // 打开文件与显示/编辑代码并排按钮布局
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(6);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    btnOpenFile = new QPushButton(tr("打开"), this);
    connect(btnOpenFile, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
    
    btnToggleEditor = new QPushButton(tr("编辑"), this);
    
    btnLayout->addWidget(btnOpenFile);
    btnLayout->addWidget(btnToggleEditor);
    
    sidebarLayout->addLayout(btnLayout);
    
    // 已打开文件列表
    fileList = new QListWidget(this);
    sidebarLayout->addWidget(fileList);
    
    splitter->addWidget(leftSidebar);
    
    // 2.2 源码编辑器（作为 graphicsView 的子窗口悬浮展示，不放入布局）
    editor = new QPlainTextEdit(graphicsView);
    editor->setPlaceholderText(tr("在此输入 PlantUML 代码..."));
    editor->setCenterOnScroll(true);
    new PumlHighlighter(editor->document());
    
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(editor);
    shadow->setBlurRadius(15);
    shadow->setColor(QColor(0, 0, 0, 45));
    shadow->setOffset(4, 0);
    editor->setGraphicsEffect(shadow);
    
    // 2.3 右侧栏：视口与 Tab 页签栏
    QWidget *rightContainer = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_diagramTabBar = new QTabBar(rightContainer);
    m_diagramTabBar->setDrawBase(false);
    m_diagramTabBar->hide();

    rightLayout->addWidget(m_diagramTabBar);
    rightLayout->addWidget(graphicsView);

    splitter->addWidget(rightContainer);
    
    connect(m_diagramTabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
    
    // 设定初始分割比例：最左栏 150px，右栏占满整个视口空间
    QList<int> sizes;
    sizes << 150 << 1150;
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
        QTabBar {
            background-color: #ffffff;
            border-bottom: 1px solid #e5e7eb;
        }
        QTabBar::tab {
            background-color: transparent;
            color: #71717a;
            padding: 8px 16px;
            font-weight: 500;
            font-size: 13px;
            border: none;
            border-bottom: 2px solid transparent;
        }
        QTabBar::tab:hover {
            color: #3f3f46;
            background-color: #f4f4f5;
        }
        QTabBar::tab:selected {
            color: #6366f1;
            border-bottom: 2px solid #6366f1;
            font-weight: bold;
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

void MainWindow::onRenderFinished(const QStringList &titles, const QVector<ParseError> &errors)
{
    // 1. 临时阻断信号，避免 clear() 或 addTab() 触发 currentChanged
    m_diagramTabBar->blockSignals(true);
    while (m_diagramTabBar->count() > 0) {
        m_diagramTabBar->removeTab(0);
    }
    for (const auto &title : titles) {
        m_diagramTabBar->addTab(title);
    }
    
    // 若单图或无图，隐藏页签栏；多图时显示并选中第 0 页
    if (titles.size() <= 1) {
        m_diagramTabBar->hide();
    } else {
        m_diagramTabBar->setCurrentIndex(0);
        m_diagramTabBar->show();
    }
    m_diagramTabBar->blockSignals(false);

    // 2. 更新底部的语法错误/就绪状态显示
    if (errors.isEmpty()) {
        statusLabel->setText(tr("就绪"));
        statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");
    } else {
        const auto &err = errors.first();
        statusLabel->setText(QString(tr("语法错误：第 %1 行 - %2")).arg(err.location.line).arg(err.message));
        statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
    }

    if (m_pendingAutoFit) {
        m_pendingAutoFit = false;
        fitView();
    }
}

void MainWindow::onTabChanged(int index)
{
    if (index >= 0) {
        renderController->renderDiagramIndex(index);
    }
}

void MainWindow::onItemActivated(QString semanticId, SourceLocation location)
{
    qDebug() << "[MainWindow] 点击激活图元 ID:" << semanticId << "行号:" << location.line;
    
    if (location.line <= 0) return;
    
    // 只有在编辑器本身可见时，才执行代码高亮和聚焦。若处于隐藏收起状态，则不自动展开
    if (!m_editorVisible) {
        return;
    }
    
    QTextBlock block = findSemanticBlock(editor, semanticId);
    if (block.isValid()) {
        qDebug() << "[MainWindow] 通过 semanticId 命中 block:"
                 << "semanticId" << semanticId
                 << "blockNumber" << (block.blockNumber() + 1)
                 << "text" << block.text();
    } else {
        block = editor->document()->findBlockByLineNumber(location.line - 1);
    }

    if (block.isValid()) {
        qDebug() << "[MainWindow] 目标 block:"
                 << "requestedLine" << location.line
                 << "blockNumber" << (block.blockNumber() + 1)
                 << "position" << block.position()
                 << "length" << block.length()
                 << "text" << block.text();

        QTextCursor cursor(block);
        
        // 选中这一整行，反白显示，增加视觉聚焦提示
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
        editor->setFocus();
        editor->centerCursor();
        editor->ensureCursorVisible();

        qDebug() << "[MainWindow] editor cursor:"
                 << "selectionStart" << editor->textCursor().selectionStart()
                 << "selectionEnd" << editor->textCursor().selectionEnd()
                 << "cursorRect" << editor->cursorRect()
                 << "vScroll" << editor->verticalScrollBar()->value();
    } else {
        qWarning() << "[MainWindow] 无法找到目标 block，requestedLine:" << location.line
                   << "documentBlockCount:" << editor->document()->blockCount();
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
        if (it.value().filePath == QFileInfo(filePath).absoluteFilePath()) {
            // 如果此文件在外部被删除且现已恢复，重置其删除标志并重新启用监听
            if (it.value().isDeleted) {
                it.value().isDeleted = false;
                it.key()->setText(QFileInfo(filePath).fileName());
                it.key()->setForeground(QBrush());
                fileWatcher->addPath(it.value().filePath);
            }
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
    openedFile.filePath = fileInfo.absoluteFilePath();
    openedFile.content = content;
    m_files[newItem] = openedFile;
    
    // 启用外部文件系统监控
    fileWatcher->addPath(openedFile.filePath);
    
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
        m_pendingAutoFit = true;
        renderController->setSourceText(openedFile.content);

        m_justChangedFile = true;
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

void MainWindow::showFileListContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = fileList->itemAt(pos);
    if (!item) return;

    // 自动将右键点击项设为当前活跃文件并加载其视图
    fileList->setCurrentItem(item);

    QMenu menu(this);
    QAction *exportAct = menu.addAction(tr("导出布局反馈 JSON"));
    connect(exportAct, &QAction::triggered, this, &MainWindow::exportLayoutFeedback);
    
    menu.exec(fileList->mapToGlobal(pos));
}

void MainWindow::exportLayoutFeedback()
{
    QString defaultName = "layout_feedback.json";
    if (m_currentListItem && m_files.contains(m_currentListItem)) {
        QFileInfo fi(m_files[m_currentListItem].filePath);
        if (!fi.baseName().isEmpty()) {
            defaultName = fi.baseName() + "_layout_feedback.json";
        }
    }

    QString savePath = QFileDialog::getSaveFileName(
        this,
        tr("导出布局反馈 JSON"),
        defaultName,
        tr("JSON 文件 (*.json)")
    );
    
    if (savePath.isEmpty()) return;

    QJsonObject rootObj;
    rootObj["width"] = graphicsScene->sceneRect().width();
    rootObj["height"] = graphicsScene->sceneRect().height();
    if (m_currentListItem && m_files.contains(m_currentListItem)) {
        rootObj["sourceFile"] = m_files[m_currentListItem].filePath;
    }

    QJsonArray nodesArray;
    QJsonArray packagesArray;
    QJsonArray relationsArray;

    QList<QGraphicsItem*> allItems = graphicsScene->items();
    for (auto *item : allItems) {
        if (auto *box = dynamic_cast<ClassBoxItem*>(item)) {
            QJsonObject nodeObj;
            nodeObj["id"] = box->id();
            
            QGraphicsItem *parent = box->parentItem();
            if (parent) {
                if (auto *parentPkg = dynamic_cast<PackageGroupItem*>(parent)) {
                    nodeObj["parentPackageId"] = parentPkg->id();
                }
            } else {
                nodeObj["parentPackageId"] = "";
            }

            QJsonObject initGeo;
            initGeo["x"] = box->initialPos().x();
            initGeo["y"] = box->initialPos().y();
            initGeo["width"] = box->size().width();
            initGeo["height"] = box->size().height();
            nodeObj["initial"] = initGeo;

            QJsonObject adjGeo;
            adjGeo["x"] = box->currentRect().x();
            adjGeo["y"] = box->currentRect().y();
            adjGeo["width"] = box->currentRect().width();
            adjGeo["height"] = box->currentRect().height();
            nodeObj["adjusted"] = adjGeo;

            nodesArray.append(nodeObj);
        }
        else if (auto *pkg = dynamic_cast<PackageGroupItem*>(item)) {
            QJsonObject pkgObj;
            pkgObj["id"] = pkg->id();

            QJsonObject initGeo;
            initGeo["x"] = pkg->originalRect().x();
            initGeo["y"] = pkg->originalRect().y();
            initGeo["width"] = pkg->originalRect().width();
            initGeo["height"] = pkg->originalRect().height();
            pkgObj["initial"] = initGeo;

            QJsonObject adjGeo;
            adjGeo["x"] = pkg->currentRect().x();
            adjGeo["y"] = pkg->currentRect().y();
            adjGeo["width"] = pkg->currentRect().width();
            adjGeo["height"] = pkg->currentRect().height();
            pkgObj["adjusted"] = adjGeo;

            packagesArray.append(pkgObj);
        }
        else if (auto *rel = dynamic_cast<RelationItem*>(item)) {
            QJsonObject relObj;
            relObj["fromNodeId"] = rel->fromNodeId();
            relObj["toNodeId"] = rel->toNodeId();
            relObj["label"] = rel->label();

            QJsonObject initGeo;
            initGeo["startX"] = rel->initialStart().x();
            initGeo["startY"] = rel->initialStart().y();
            initGeo["endX"] = rel->initialEnd().x();
            initGeo["endY"] = rel->initialEnd().y();
            relObj["initial"] = initGeo;

            QJsonObject adjGeo;
            adjGeo["startX"] = rel->currentStart().x();
            adjGeo["startY"] = rel->currentStart().y();
            adjGeo["endX"] = rel->currentEnd().x();
            adjGeo["endY"] = rel->currentEnd().y();
            relObj["adjusted"] = adjGeo;

            relationsArray.append(relObj);
        }
    }

    rootObj["nodes"] = nodesArray;
    rootObj["packages"] = packagesArray;
    rootObj["relations"] = relationsArray;

    QJsonDocument doc(rootObj);
    
    QFile file(savePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        QMessageBox::information(this, tr("导出成功"), tr("布局反馈数据已成功导出为 JSON 文件！请将其发送给 AI 进行分析。"));
    } else {
        QMessageBox::warning(this, tr("导出失败"), tr("无法写入反馈 JSON 文件，请检查权限。"));
    }
}

void MainWindow::setEditorVisible(bool visible, bool animate)
{
    m_editorVisible = visible;

    if (btnToggleEditor) {
        btnToggleEditor->setText(visible ? tr("收起") : tr("编辑"));
    }

    if (m_animation) {
        m_animation->stop();
    }

    // 计算自适应展开宽度：限定在 350px - 500px 之间，为 graphicsView 宽度的 40%
    int targetEditorWidth = qBound(350, int(graphicsView->width() * 0.4), 500);

    if (!animate) {
        if (visible) {
            editor->show();
            editor->setGeometry(0, 0, targetEditorWidth, graphicsView->height() / 2);
            editor->raise();
        } else {
            editor->hide();
            editor->setGeometry(0, 0, 0, graphicsView->height() / 2);
        }
        return;
    }

    int startWidth = editor->width();
    int endWidth = visible ? targetEditorWidth : 0;

    if (visible) {
        editor->show();
        // 强制初始化为 0 宽，防止刚 show 时闪烁出默认大小
        editor->setGeometry(0, 0, 0, graphicsView->height() / 2);
        editor->raise();
        startWidth = 0;
    }

    m_animation = new QVariantAnimation(this);
    m_animation->setDuration(250); // 250ms 的过渡动画，利落而优雅
    m_animation->setStartValue(startWidth);
    m_animation->setEndValue(endWidth);
    m_animation->setEasingCurve(QEasingCurve::OutCubic); // 优雅的减速缓动效果

    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        int width = value.toInt();
        editor->setGeometry(0, 0, width, graphicsView->height() / 2);
    });

    connect(m_animation, &QVariantAnimation::finished, this, [this, visible]() {
        if (!visible) {
            editor->hide(); // 彻底隐藏，防事件穿透
        }
    });

    m_animation->start(QAbstractAnimation::DeleteWhenStopped);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == graphicsView && event->type() == QEvent::Resize) {
        // 当右侧视图大小改变时，同步调整悬浮编辑器的尺寸，使其高度占满一半，宽度维持自适应比例
        if (editor) {
            if (m_editorVisible) {
                int targetEditorWidth = qBound(350, int(graphicsView->width() * 0.4), 500);
                editor->setGeometry(0, 0, targetEditorWidth, graphicsView->height() / 2);
            } else {
                editor->setGeometry(0, 0, 0, graphicsView->height() / 2);
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onFileChanged(const QString &path)
{
    QListWidgetItem *targetItem = nullptr;
    for (auto it = m_files.begin(); it != m_files.end(); ++it) {
        if (it.value().filePath == path) {
            targetItem = it.key();
            break;
        }
    }
    
    if (!targetItem) return;
    
    if (!QFile::exists(path)) {
        // 文件已在外部被删除
        m_files[targetItem].isDeleted = true;
        targetItem->setText(QFileInfo(path).fileName() + tr(" (已在外部删除)"));
        targetItem->setForeground(QBrush(QColor("#ef4444"))); // 使用淡红色高亮展示已删除状态
        
        // 从监听器中注销该物理路径的监听
        fileWatcher->removePath(path);
        return;
    }
    
    // 外部文件被修改，读取新数据
    QFile file(path);
    // 尝试以 50ms 间隔至多重试 5 次，规避大部分外部编辑器写回时造成的瞬间独占锁
    int retries = 5;
    bool opened = false;
    while (retries > 0) {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            opened = true;
            break;
        }
        QThread::msleep(50);
        retries--;
    }
    
    if (!opened) return;
    
    QTextStream in(&file);
    QString newContent = in.readAll();
    file.close();
    
    m_files[targetItem].content = newContent;
    m_files[targetItem].isDeleted = false;
    
    // 恢复列表项状态样式
    targetItem->setText(QFileInfo(path).fileName());
    targetItem->setForeground(QBrush()); // 恢复默认前景色
    
    // 若当前编辑器展示的文件即为该发生变化的文件，实时同步渲染
    if (m_currentListItem == targetItem) {
        editor->blockSignals(true);
        editor->setPlainText(newContent);
        editor->blockSignals(false);
        
        renderController->setSourceText(newContent);
    }
}

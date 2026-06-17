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
    
    // 4. 尝试从本地探测并加载默认的测试用例文件，彻底清空 C++ 代码中的硬编码内容
    QVector<QString> fileNames = {"untitled.puml", "class_diagram.puml"};
    QListWidgetItem *firstLoadedItem = nullptr;
    
    for (const auto &fileName : fileNames) {
        QString resolvedPath = "";
        QString content = "";
        
        QVector<QString> candidatePaths = {
            fileName,
            "../" + fileName,
            QCoreApplication::applicationDirPath() + "/" + fileName,
            QCoreApplication::applicationDirPath() + "/../" + fileName
        };
        
        for (const auto &path : candidatePaths) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                content = in.readAll();
                file.close();
                resolvedPath = path;
                break;
            }
        }
        
        if (!resolvedPath.isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem(fileName, fileList);
            OpenedFile openedFile;
            openedFile.filePath = QFileInfo(resolvedPath).absoluteFilePath();
            openedFile.content = content;
            m_files[item] = openedFile;
            
            if (!firstLoadedItem) {
                firstLoadedItem = item;
            }
        }
    }
    
    // 激活第一个成功加载的文件 (这会自动触发 onCurrentFileChanged 进行首次解析绘制)
    if (firstLoadedItem) {
        fileList->setCurrentItem(firstLoadedItem);
    }
    
    // 启用列表项的自定义右键菜单并绑定信号槽
    fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(fileList, &QListWidget::customContextMenuRequested, this, &MainWindow::showFileListContextMenu);
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
    editor->setCenterOnScroll(true);
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

    if (m_pendingAutoFit) {
        m_pendingAutoFit = false;
        fitView();
    }
}

void MainWindow::onItemActivated(QString semanticId, SourceLocation location)
{
    qDebug() << "[MainWindow] 点击激活图元 ID:" << semanticId << "行号:" << location.line;
    
    if (location.line <= 0) return;
    
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
        m_pendingAutoFit = true;
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

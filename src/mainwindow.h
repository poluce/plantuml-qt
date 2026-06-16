#pragma once

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QTextBlock>
#include <QListWidget>
#include <QHash>
#include "graphics/DiagramScene.h"
#include "graphics/DiagramView.h"
#include "app/RenderController.h"
#include "parser/ParseError.h"

// 已打开文件的数据模型
struct OpenedFile
{
    QString filePath;   // 本地绝对路径 (新建未保存文件则为空)
    QString content;    // 内存中的最新源码内容
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 文本内容更改槽
    void onTextChanged();
    
    // 编译渲染完毕槽
    void onRenderFinished(const QVector<ParseError> &errors);
    
    // 图元选中触发槽
    void onItemActivated(QString semanticId, SourceLocation location);
    
    // 复位视口
    void resetView();
    
    // 适应视口
    void fitView();

    // 点击“打开文件”按钮触发槽
    void onOpenFileClicked();

    // 项目列表中选择项改变触发槽
    void onCurrentFileChanged(QListWidgetItem *current, QListWidgetItem *previous);

private:
    void setupUi();       // 构建三栏拉伸布局
    void setupStyles();   // 现代浅色主题 QSS

    // 三栏界面控件
    QSplitter *splitter;
    
    // 最左栏：项目管理
    QWidget *leftSidebar;
    QPushButton *btnOpenFile;
    QListWidget *fileList;
    
    // 中间栏：源码编辑器
    QPlainTextEdit *editor;
    
    // 右栏：视口
    DiagramView *graphicsView;
    DiagramScene *graphicsScene;
    
    // 工具栏状态与按钮
    QLabel *statusLabel;
    QPushButton *btnReset;
    QPushButton *btnFit;
    QPushButton *btnRefresh;

    // 控制层与文件哈希绑定
    RenderController *renderController;
    QHash<QListWidgetItem*, OpenedFile> m_files;
    QListWidgetItem *m_currentListItem; // 跟踪当前活跃的项目列表项
};

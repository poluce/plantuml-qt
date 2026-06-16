#pragma once

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QTextBlock>
#include "graphics/DiagramScene.h"
#include "graphics/DiagramView.h"
#include "app/RenderController.h"
#include "parser/ParseError.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 文本输入改变，通报给控制器
    void onTextChanged();
    
    // 控制器编译渲染完毕回调，用于更新状态栏和错误列表
    void onRenderFinished(const QVector<ParseError> &errors);
    
    // 拦截右侧画布点击，跳转高亮左侧编辑器源码行
    void onItemActivated(QString semanticId, SourceLocation location);
    
    // 复位缩放 1:1
    void resetView();
    
    // 自适应视口，显示整张时序图
    void fitView();

private:
    void setupUi();       // 构建双栏交互及工具栏
    void setupStyles();   // 设定高颜值现代浅色 QSS 样式表

    // UI 控件
    QSplitter *splitter;
    QPlainTextEdit *editor;
    DiagramView *graphicsView;
    DiagramScene *graphicsScene;
    
    QLabel *statusLabel;
    QPushButton *btnReset;
    QPushButton *btnFit;
    QPushButton *btnRefresh;

    // 本地控制器
    RenderController *renderController;
};

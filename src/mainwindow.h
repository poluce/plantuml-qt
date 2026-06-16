#pragma once

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QGraphicsScene>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include "plantuml_view.h"
#include "plantuml_encoder.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 编辑框输入改变响应
    void onTextChanged();
    
    // 防抖计时器触发
    void onRenderTimerTriggered();
    
    // SVG 网络数据下载完成回调
    void onNetworkReplyFinished(QNetworkReply *reply);
    
    // 重置缩放
    void resetView();

private:
    void setupUi();       // 初始化双栏与工具栏布局
    void setupStyles();   // 注入现代暗黑 QSS 视觉体系

    // 界面控件
    QSplitter *splitter;
    QPlainTextEdit *editor;
    PlantUmlView *graphicsView;
    QGraphicsScene *graphicsScene;
    
    QLabel *statusLabel;
    QPushButton *btnReset;
    QPushButton *btnRefresh;

    // 后台组件
    QTimer *renderTimer;
    QNetworkAccessManager *networkManager;
    PlantUmlEncoder *encoder;
};

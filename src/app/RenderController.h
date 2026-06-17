#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QVector>
#include "../graphics/DiagramScene.h"
#include "../graphics/DiagramSceneRenderer.h"
#include "../graphviz/GraphvizLayoutEngine.h"
#include "../render_model/RenderTheme.h"
#include "../parser/ParseError.h"

class RenderController : public QObject
{
    Q_OBJECT
public:
    explicit RenderController(DiagramScene *scene, QObject *parent = nullptr);

    // 触发防抖更新
    void setSourceText(const QString &text);

signals:
    // 解析与布局结束后通知界面，传递可能存在的语法错误
    void renderFinished(const QVector<ParseError> &errors);

private slots:
    // 执行真正的渲染计算流程
    void doRender();

private:
    DiagramScene *m_scene = nullptr;
    QString m_sourceText;
    QTimer m_debounceTimer;
    RenderTheme m_theme;
    
    GraphvizLayoutEngine m_layoutEngine;
    DiagramSceneRenderer m_renderer;
};

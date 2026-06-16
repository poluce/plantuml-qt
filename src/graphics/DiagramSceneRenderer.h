#pragma once

#include "DiagramScene.h"
#include "../render_model/RenderDocument.h"
#include "../render_model/RenderTheme.h"

class DiagramSceneRenderer
{
public:
    // 将 RenderDocument 承载的几何模型渲染挂载到特定的 DiagramScene 上
    void render(DiagramScene *scene, const RenderDocument &doc, const RenderTheme &theme);
};

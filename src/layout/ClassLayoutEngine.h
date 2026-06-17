#pragma once

#include "LayoutEngine.h"

class ClassLayoutEngine : public LayoutEngine
{
public:
    // 计算类图 AST，结算所有类卡片和连线在画布上的拓扑布局坐标，返回 RenderDocument
    RenderDocument layout(const ClassDiagramAst &ast, const RenderTheme &theme);
};

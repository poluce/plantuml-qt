#pragma once

#include "LayoutEngine.h"

class SequenceLayoutEngine : public LayoutEngine
{
public:
    // 根据时序图 AST，执行几何布局，计算所有的节点和边坐标，返回与渲染无关的 RenderDocument
    RenderDocument layout(const SequenceDiagramAst &ast, const RenderTheme &theme);
};

# plantuml-qt

基于 Qt6 Widgets、QGraphicsScene 和 Graphviz 的 PlantUML 桌面查看器。

## 功能特性
- **本地解析**：使用项目内置 Parser 解析 PlantUML 子集，生成 AST 语义结构。
- **Graphviz 排版**：将 AST 转换为通用 LayoutGraph，再由 `dot -Tjson` 自动计算节点坐标、边路径和文本位置。
- **自绘渲染**：Graphviz 只负责排版，最终图形仍由 `QGraphicsScene` 和自定义 `QGraphicsItem` 绘制。
- **流畅交互**：支持多文件编辑、鼠标平移和滚轮缩放。

## 渲染架构

当前渲染流水线：

```text
PlantUML 文本
  ↓
Parser
  解析 PlantUML 文本
  ↓
AST
  保存语义结构，例如 class、interface、relation、message
  ↓
LayoutGraph
  把 AST 转成通用图结构，例如 node、edge、rank、direction、label
  ↓
DotBuilder
  把 LayoutGraph 转成 DOT 语言
  ↓
dot -Tjson
  Graphviz 负责自动排版，输出节点坐标、边路径、文本位置等
  ↓
GraphLayoutResult
  解析 Graphviz JSON 的结果
  ↓
RenderDocument
  转成项目自己的渲染模型，保存 QRectF、QPainterPath、文本、样式、源码行号
  ↓
QGraphicsScene
  根据 RenderDocument 创建 QGraphicsItem
```

这个架构的边界是：Parser 和 AST 负责语义，Graphviz 负责排版，Qt 负责最终视觉绘制与交互。

## 开发与构建

项目采用 out-of-source 编译，请在独立的 `build` 文件夹中进行编译，禁止在项目根目录下直接编译。

### 运行依赖

需要本机已安装 Graphviz，并且 `dot` 可从 `PATH` 中直接调用：

```bash
dot -V
```

### 编译步骤
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

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
Parser (PumlParser)
  解析 PlantUML 文本并分类图
  ↓
AST (DiagramAst)
  保存语义结构，例如 class、interface、relation、message
  ↓
LayoutGraph
  把 AST 转成通用图结构，例如 node、edge、rank、direction、label
  ↓
DotBuilder & DotProcessRunner
  把 LayoutGraph 转成 DOT 语言并通过 dot -Tjson 启动排版
  ↓
GraphLayoutResult
  解析 Graphviz 布局计算后的 JSON 结果（包含节点坐标与边曲线）
  ↓
RenderDocument
  转成项目自身的渲染模型，保存 QRectF、QPainterPath、文本、样式、源码行号
  ↓
QGraphicsScene & QGraphicsItem
  根据 RenderDocument 创建 QGraphicsItem（例如 ClassBoxItem、RelationItem 等）
```

这个架构的边界是：Parser 和 AST 负责语义，Graphviz 负责排版，Qt 负责最终视觉绘制与交互。

## 核心设计与重构决策（面向后续开发）

为了让下一个开发实例/维护者能快速接班，以下是项目关键部分的重构设计原则与实现逻辑：

### 1. 行命令解析模式 (LineCommand Pattern)
- **设计动机**：摒弃了传统的 Lexer-Tokenizer 细粒度分词，采用按行扫描匹配的**行命令模式**。由于 PlantUML 是一种典型的面向行（line-oriented）的声明式语言，每一行通常代表一个独立的元数据或关系声明。
- **实现细节**：
  - 抽象出 `LineCommand` 基类。每个解析职责对应一个独立的、高内聚的 Command 子类（例如 `ClassDeclCommand`、`HyphenRelationCommand` 等）。
  - 在 `PumlParser::parse()` 时，所有命令实例以 `QVector` 指针链形式按顺序进行匹配。
- **图类别分流机制**：
  - 在解析开始前，执行首趟轻量级“前瞻匹配”：扫描是否有 `class `、`interface ` 或 `-up->` 等类图专属特征。
  - 如果包含这些特征，则将图类别判定为类图，并在堆上实例化 `ClassDiagramAst`；否则默认为时序图（`SequenceDiagramAst`）。
  - 不同图类型的解析链会跳过不属于其类型的行（例如，时序图会跳过类定义，类图会跳过 `participant`）。

### 2. 物理行号对齐机制
- **问题重现**：以前版本的行切分使用的是 `split(QRegularExpression("[\r\n]+"))`，这会导致连续的空行被合并。导致解析器内部行索引比 IDE 编辑器里的物理行号小，报错时行号错位（例如物理第 229 行的连线报错，在界面上被标记为了第 218 行）。
- **解决方案**：修改为 `split('\n')` 物理分行，并在命令匹配循环里通过 `trimmed().isEmpty()` 仅在逻辑上跳过空行，保留空行产生的索引占位。这使得解析器抛出的 `SourceLocation::line` **100% 精准对齐物理行号**。

### 3. 关系连线解析子模块化
为了应对复杂的 PlantUML 连线符号，解析器将连线拆分为 4 大类高内聚的行解析器：
1. `StandardRelationCommand`：匹配标准连线（如 `-->`、`->`）及带右括号的方向标识（如 `-->[up]`）。
2. `HyphenRelationCommand`：匹配夹带方向的实线关联（如 `-up->`、`--left->` 等）。
3. `DotRelationCommand`：匹配夹带方向的虚线依赖（如 `.up.>`、`..left.` 等）。
4. `SpecialRelationCommand`：匹配特殊的类继承、聚合、组合连线（如 `<|--up-`、`o-left->`）。

---

## 信息源与技术参考

在后续维护、修改或扩展本项目的语法支持时，请务必参考以下官方规范与原理：

### 1. PlantUML 规范参考
- **类图语法**：[PlantUML Class Diagram Official Guide](https://plantuml.com/zh/class-diagram)
  - 注意连线语法中，支持在关系符号中间嵌入方向，如 `-up->`，也支持在关系符号后使用方括号，如 `-->[up]`。
  - 图的开始与结束标签：必须支持 `@startuml <Name>` （可以带可选的图名参数）和尾部的结束标志（兼容 `@enduml` 和 `@endum`）。
- **时序图语法**：[PlantUML Sequence Diagram Official Guide](https://plantuml.com/zh/sequence-diagram)
  - 隐式实体声明：如果消息发送方/接收方没有被 `participant` 声明过，解析器需要支持隐式创建它们（`ensureParticipantExists`）。

### 2. Graphviz 布局与死锁破除
- **布局输出格式**：项目调用了 `dot -Tjson`。在处理 Graphviz 输出的 JSON 时，读取了 `node` 中的 `pos`（中心坐标）以及 `edge` 中的 `pos`（包含 B-Spline 控制点列表的路径数据）。
- **方向环路死锁与起止点翻转**：
  - Graphviz `dot` 在做分层（rank）布局时，对于 `up` 和 `left` 类型的边约束容易与主绘图流向产生环路死锁。
  - **解决之道**：在 `GraphvizLayoutEngine.cpp` 构建阶段，直接将 `up` 和 `left` 边的起止点（`from` 和 `to`）在物理上进行对调，并将方向转义为 `down` 和 `right`。通过在 DOT 层面将边逆向，消除了反向 `constraint` 约束，彻底避免了布局引擎的死锁。

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

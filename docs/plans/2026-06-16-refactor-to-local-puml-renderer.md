# Local Text-Driven PlantUML Renderer Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 实现一个完全在本地运行的 PlantUML 时序图解析与渲染引擎，将原有的在线下载方案重构为纯本地的 `Parser -> AST -> Layout -> RenderModel -> QGraphicsScene` 编译流水线。

**Architecture:** 
1. **parser/ (解析层)**：手写 Lexer 与 Parser，将 `.puml` 源码解析为 Token 序列，生成语法树并收集编译错误。
2. **ast/ (语义模型)**：定义 `SequenceDiagramAst` 用于存储时序图的语义数据（如参与者和消息调用）。
3. **layout/ (布局计算)**：`SequenceLayoutEngine` 计算图元三维几何，为参与者分配 Lifeline 的 X 轴，消息按纵向时序分配 Y 轴，输出 RenderModel。
4. **render_model/ (渲染模型)**：与 Qt 绘图层无关的数据定义（`RenderDocument`、`RenderNode`、`RenderEdge` 等）。
5. **graphics/ (图形渲染)**：自定义 `DiagramView` 处理平移缩放；`DiagramSceneRenderer` 将渲染模型翻译为聚合图元 `ParticipantItem` 与 `MessageArrowItem`。
6. **app/ (控制核心)**：`RenderController` 负责防抖排程与流水线运转。

---

### Task 1: 声明底层数据结构与解析系统 (Parser & AST)

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/SourceLocation.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/ParseError.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/Token.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/PumlLexer.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/PumlLexer.cpp`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/PumlParser.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/PumlParser.cpp`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/ast/DiagramAst.h`

**Step 1: 创建基本实体和 Token 体系**
设计 Token 类型（StartUml, EndUml, Participant, Arrow, Colon 等）和源码定位 SourceLocation。

**Step 2: 实现 PumlLexer**
分词扫描 `.puml` 文本，处理行号和字符下标。

**Step 3: 实现 PumlParser 并定义 AST**
递归解析 Token 序列，生成包含 `participants` 和 `messages` 的 `SequenceDiagramAst`，并收集潜在的语法错误。

---

### Task 2: 建立布局系统与渲染数据模型 (Layout & RenderModel)

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/render_model/RenderDocument.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/render_model/RenderTheme.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/layout/LayoutEngine.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/layout/SequenceLayoutEngine.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/layout/SequenceLayoutEngine.cpp`

**Step 1: 创建 RenderDocument**
定义与 Qt 绘图逻辑解耦的 `RenderNode` (矩形节点) 和 `RenderEdge` (连线路径) 模型。

**Step 2: 编写 SequenceLayoutEngine**
实现布局数学公式：
- 参与者根据出现顺序在横向以 `participantSpacing` 排开。
- 消息在纵向按行时序排列，递增累加 Y 坐标。
- 生成包含节点、连线与生命线轨迹的 RenderDocument。

---

### Task 3: 实现图形呈现与聚合图元绘制 (Graphics)

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/items/ParticipantItem.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/items/ParticipantItem.cpp`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/items/MessageArrowItem.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/items/MessageArrowItem.cpp`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/DiagramScene.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/DiagramScene.cpp`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/DiagramSceneRenderer.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/DiagramSceneRenderer.cpp`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/DiagramView.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/DiagramView.cpp`

**Step 1: 编写 ParticipantItem 与 MessageArrowItem**
继承自 `QGraphicsItem`。一个 ParticipantItem 负责绘制头部矩形框、名字文本，并向下延伸绘制虚线 Lifeline，实现图元的高内聚低耦合。MessageArrowItem 负责画水平消息箭头和标签文本。

**Step 2: 编写 DiagramScene 与 DiagramView**
Scene 负责保存图元并维护 id 到 item 的双向跳转索引。View 拦截鼠标和滚轮事件，执行平移与中心缩放。

**Step 3: 编写 DiagramSceneRenderer**
遍历 `RenderDocument`，为 Scene 添加或更新对应的 GraphicsItem，实现渲染映射。

---

### Task 4: 编写流水线控制器并联调主界面 (App)

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/app/RenderController.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/app/RenderController.cpp`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/mainwindow.h`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/mainwindow.cpp`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/main.cpp`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/CMakeLists.txt`

**Step 1: 编写 RenderController**
编排渲染流水线：文本输入变化后防抖 -> Parser 解析 -> Layout 布局 -> Renderer 重建 QGraphicsScene。

**Step 2: 联调 MainWindow**
将左侧 `QPlainTextEdit`、右侧 `DiagramView` 通过 `RenderController` 绑定起来。在底部或工具栏新增一个“错误提示”区，解析发生错误时显示红字提醒。

**Step 3: 修改 CMakeLists.txt**
将上述新建的所有源文件编入 `CMakeLists.txt`，移除不再使用的 `plantuml_encoder`（不再需要网络请求和 Base64 压缩）。

---

### Task 5: 独立目录编译与系统测试

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/build/`

**Step 1: 清理缓存并编译**
Run: `if (Test-Path build) { Remove-Item -Recurse -Force build }; cmake -G \"MinGW Makefiles\" -B build -S .; cmake --build build`
Expected: 编译完美成功。

**Step 2: 功能与性能验证**
测试在左侧输入时序图，右侧立即在本地完成渲染；测试 `Ctrl + 滚轮` 进行无级放大缩小；测试故意输入错误语法时，界面右侧或状态栏能给出错误红字提醒。

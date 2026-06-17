# Implement Class Diagram Feature Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 扩展本地的 PlantUML 解析与布局渲染系统，使其支持最简类图子集（类声明、属性方法多行定义及继承/关联连线关系），并能根据类继承深度自动执行垂直层级（Layer）拓扑布局计算。

**Architecture:** 
1. **语法分析扩充**：在 `TokenType` 增加 `Class`、`LeftBrace` (`{`)、`RightBrace` (`}`) 以及继承连线 `<|--` 等。
2. **抽象语法树**：在 `DiagramAst.h` 增加 `ClassDiagramAst`。
3. **分流计算**：`RenderController` 根据 AST 的具体类型，智能分流给 `SequenceLayoutEngine` 或 `ClassLayoutEngine`。
4. **类图布局算法**：`ClassLayoutEngine` 对类节点进行入度/层级（Layer）深度计算。继承链上游（父类）置于上方，下游（子类）置于下方。
5. **图形绘制**：新增 `ClassBoxItem`（采用 QPainter 绘制双层类信息卡片）和 `RelationItem`（绘制继承空心三角箭头或普通指向连线）。

---

### Task 1: 扩充 Token 记号与 AST 语义模型

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/Token.h`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/ast/DiagramAst.h`

**Step 1: 在 Token.h 中新增记号**
新增 `Class`、`LeftBrace`、`RightBrace`、`Inherit` 类型。

**Step 2: 在 DiagramAst.h 中定义类图 AST**
定义 `ClassDecl` (包含类名与多行 `QVector<QString> members` 成员字段) 与 `RelationDecl` (包含关联/继承连线关系)。定义主类 `ClassDiagramAst` 并设计虚函数 `isSequence()` 进行运行时分流。

---

### Task 2: 扩充 Lexer 与 Parser 支持类图语法

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/PumlLexer.cpp`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/PumlParser.h`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/PumlParser.cpp`

**Step 1: 在 PumlLexer.cpp 中新增分词分支**
处理 `{`、`}`、`<|--` 以及单词 `class` 并映射为对应 Token。

**Step 2: 重构 PumlParser.cpp**
支持解析：
- `class ClassName { ... }` 块，并将大括号内部的每行属性或方法捕获。
- `ClassA <|-- ClassB` 和 `ClassA --> ClassB`。
- 返回统一基类指针 `std::unique_ptr<DiagramAst>`。

---

### Task 3: 编写类图布局引擎 (ClassLayoutEngine)

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/layout/ClassLayoutEngine.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/layout/ClassLayoutEngine.cpp`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/render_model/RenderDocument.h`

**Step 1: 扩充 RenderDocument**
在 `RenderNode` 中添加 `members` 字段；在 `RenderEdge` 中添加 `RenderEdgeKind::Inheritance` 继承选项。

**Step 2: 编写 ClassLayoutEngine**
计算拓扑层级（Layer）：
- 查找无父类的类，设定 Layer = 0。
- 若 ClassB 继承自 ClassA，则 Layer(ClassB) = Layer(ClassA) + 1。
- 同一 Layer 的类在横向按间距 220px 平开，不同 Layer 在纵向按 Y 轴下落 200px。
- 输出定位后的 `RenderDocument`。

---

### Task 4: 实现 ClassBoxItem 与 RelationItem 图形图元

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/items/ClassBoxItem.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/items/ClassBoxItem.cpp`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/items/RelationItem.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/items/RelationItem.cpp`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/DiagramScene.cpp`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/graphics/DiagramSceneRenderer.cpp`

**Step 1: 实现 ClassBoxItem**
绘制双层卡片：上层为类名（填充浅靛蓝背景），中部加一道细腻分割线，下层依次以 12px 锌灰写出其所有的成员属性/函数。

**Step 2: 实现 RelationItem**
如果是继承关系，在父类端（起止中点偏上）绘制一个精美的**空心白色倒三角箭头**；如果是关联关系，绘制普通分叉箭头。

**Step 3: 更新 Renderer**
在 `DiagramSceneRenderer` 中根据 `RenderNodeKind` 实例化为 `ClassBoxItem` 或 `RelationItem` 并加载到画布。

---

### Task 5: 流水线分流联调与编译验证

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/app/RenderController.h`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/app/RenderController.cpp`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/CMakeLists.txt`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/mainwindow.cpp`

**Step 1: 修改 RenderController**
在 `doRender()` 里接收 `DiagramAst` 统一指针。若 `isSequence()` 返回 true，由 `SequenceLayoutEngine` 计算；否则实例化并调用 `ClassLayoutEngine` 结算。

**Step 2: 编译与新建类图用例测试**
更新 `CMakeLists.txt` 以编译新文件，并对本地进行编译构建与功能测试。

# Implement Connection Directions (up/down/left/right) Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 扩展本地的 PlantUML 分词与排版渲染系统，使其支持带方向指示词的连线语法（例如 `-up->`、`-[left]->`、`<|-down-` 等），并在 ClassLayoutEngine 布局引擎中自适应调节拓扑分列和重心偏置，实现强制位置对齐。

**Architecture:** 
1. **分词扩充**：在 `Token` 结构体中添加 `direction` 字段。在 `PumlLexer.cpp` 中通过智能的前瞻字符串模式识别（Lookahead Substring Scan），抓取任意连线夹带的方向词（`up` / `down` / `left` / `right` 或其首字母 `u` / `d` / `l` / `r`）。
2. **语法树升级**：在 `RelationDecl` 中添加 `direction` 属性。在 `PumlParser.cpp` 关系解析中提取 Token 方向并写入 AST 节点。
3. **拓扑与排版约束应用**：在 `ClassLayoutEngine.cpp` 中：
   * **水平方向 (left/right)**：对于指定 `left` 流向的依赖，在构建 Kahn 拓扑排序的依赖图时做反向建边，使其在 X 轴上自动获得更靠左的列（Column）。
   * **垂直方向 (up/down)**：在重心引力计算时，将 `up`/`down` 翻译为卡片间的垂直 Y 轴引力偏置（如 `idealY = A.y ± 120`），迫使它们在 3 轮物理迭代中垂直排开。
4. **连线端点自适应**：在 `RelationItem` / `DiagramSceneRenderer` 中根据最终的连线类型与方向将起点/终点强行吸附在对应的卡片边缘。

---

### Task 1: 扩充 Token 属性与 AST 语义模型

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/Token.h:31-36`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/ast/DiagramAst.h:51-58`

**Step 1: 在 Token.h 中新增 direction 字段**
在 `Token` 结构体中添加 `QString direction;`，用于保存解析到的连线方向（如 `"up"`, `"down"`, `"left"`, `"right"`）。

**Step 2: 在 DiagramAst.h 中为关系添加方向属性**
在 `RelationDecl` 结构体中新增成员变量 `QString direction;`，保存连线的期望方向，以传递给布局阶段。

**Step 3: 编译验证**
运行：`cmake --build build --config Debug`
确保无编译错误。

---

### Task 2: 升级 PumlLexer.cpp 以支持方向连线的分词

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/PumlLexer.cpp`

**Step 1: 重构 PumlLexer.cpp 连线识别逻辑**
当遇到 `-`、`.`、`<`、`*`、`o` 时，我们向前抓取一整段不包含标识符的符号段落。
使用通用的字符比对模式识别连线的 `TokenType` 以及夹带的方向属性：
```cpp
// 伪代码参考：
// 1. 向前扫描获取符号串 (如 "-[up]->" 或 "<|-down-")
// 2. 判断关系种类：
//    - 包含 "<|" 且包含 "-" -> TokenType::Inherit
//    - 包含 "<|" 且包含 "." -> TokenType::Realization
//    - 包含 "*--" -> TokenType::Composition
//    - 包含 "o--" -> TokenType::Aggregation
//    - 包含 "|>" 且包含 "-" -> TokenType::InheritRight
//    - 包含 "|>" 且包含 "." -> TokenType::RealizeRight
//    - 包含 "." 且以 ">" 结尾 -> TokenType::Dependency
//    - 包含 "-" 且以 ">" 结尾 -> TokenType::Arrow / DottedArrow
// 3. 提取方向 direction：
//    - 包含 "up" 或 "u" -> "up"
//    - 包含 "down" 或 "d" -> "down"
//    - 包含 "left" or "l" -> "left"
//    - 包含 "right" or "r" -> "right"
```

**Step 2: 编译测试**
运行：`cmake --build build --config Debug`
确保编译成功。

---

### Task 3: 升级 PumlParser.cpp 提取连线方向并存入 AST

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/parser/PumlParser.cpp`

**Step 1: 提取 Token 的 direction**
在 `PumlParser::parseClassDiagram` 解析类关系的段落中，将 `relToken.direction` 赋值给 `rel.direction`。

**Step 2: 处理右向连线的方向反转**
若为右向连线（如 `InheritRight`，连线是 `--|>`），因为解析器对调了 `from` 和 `to`，其对应的显式方向（如 `up`）需要被反转（`up` ➔ `down`，`left` ➔ `right`）以保持物理关系一致。
```cpp
if (relToken.type == TokenType::InheritRight || relToken.type == TokenType::RealizeRight) {
    // 已经对调了 rel.from 和 rel.to
    if (rel.direction == "up") rel.direction = "down";
    else if (rel.direction == "down") rel.direction = "up";
    else if (rel.direction == "left") rel.direction = "right";
    else if (rel.direction == "right") rel.direction = "left";
}
```

**Step 3: 编译验证**
运行：`cmake --build build --config Debug`
确保编译无误。

---

### Task 4: 升级 ClassLayoutEngine 布局算法以响应连线方向

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/layout/ClassLayoutEngine.cpp`

**Step 1: 支持 left 方向对齐（修改拓扑建边）**
在第 111 行左右，建立包内及类与类拓扑边 `pkgAdj` 和 `adj` 时：
如果检测到 `rel.direction == "left"`，则在构建依赖边时**反向建边**（即视作由 B 指向 A），这样 Kahn 拓扑排序分列时，B 会自动被分在 A 的左侧列。

**Step 2: 支持 up/down 方向对齐（修改 Y 轴重心迭代）**
在第 667 行左右，计算类理想 Y 重心 `classIdealLocalY` 时，检查连线是否有强制方向指示：
* 若 `rel.direction == "up"`（B 在 A 上方）：
  * 计算 B 的理想 Y 时，施加向上的偏置力：`sumAbsY += (classAbsY[A] - 120.0) * weight;`
  * 计算 A 的理想 Y 时，施加向下的偏置力：`sumAbsY += (classAbsY[B] + 120.0) * weight;`
* 若 `rel.direction == "down"`（B 在 A 下方）：
  * 计算 B 理想 Y 时：`sumAbsY += (classAbsY[A] + 120.0) * weight;`
  * 计算 A 理想 Y 时：`sumAbsY += (classAbsY[B] - 120.0) * weight;`

**Step 3: 编译与运行测试**
运行：`cmake --build build --config Debug`
运行程序，并在 `class_diagram.puml` 中加入例如 `MainWindow -up-> MainController` 或是 `MainController -left-> CurveManager` 连线测试，验证其是否根据强制方向进行完美的拓扑分列和垂直拉开。

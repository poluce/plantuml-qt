# PlantUML 本地渲染器核心开发指南

## 核心开发路线

```text
官方语法文档
  ↓
PlantUML 源码验证细节
  ↓
自己定义语法子集
  ↓
写 Parser
  ↓
生成 AST
  ↓
LayoutGraph
  ↓
DotBuilder
  ↓
dot -Tjson
  ↓
GraphLayoutResult
  ↓
RenderModel
  ↓
QGraphicsScene 渲染
```

---

## 重点建议与方针

### 1. 第一版只做子集
不要追求完整的 PlantUML 语法支持。
*   **时序图优先**：布局逻辑相对简单，在无极缩放和极速响应上的性能优势最明显。
*   **简单类图辅助**：以节点和关系连线为主，适合用拓扑和网格布局。

### 2. 看源码的目的
看官方 Java 源码主要是为了**校准规则**，确认以下边界：
*   语句与关键字如何扫描识别。
*   各种箭头的表示方法（如 `->`、`-->`、`<|--` ）。
*   `participant` 别名 (`as`) 的处理逻辑。
*   `note` 多行注释的边界与如何匹配结束符。
*   `alt / loop / end` 等分支/循环控制块的匹配。
*   错误语法的容错兜底行为。
*   *看源码是为了查漏补缺和确认细节，不是为了机械抄袭完整实现。*

### 3. 定义自己的语法子集文档
这决定了 Parser 的具体边界。

**支持子集：**
*   `participant` / `actor` 及其别名定义。
*   `A -> B: message` (同步) 与 `A --> B: return` (异步/返回)。
*   `note over` / `note left of` / `note right of`。
*   `activate` / `deactivate`。
*   `alt / else / end` 与 `loop / end` 块。
*   `class ClassName { ... }` 块（支持属性和方法解析）。
*   `Base <|-- Sub` (继承连线) 与 `A --> B` (关联连线)。

**暂不支持：**
*   `!include` 与 `!define` 预处理。
*   复杂的 `skinparam` 自定义样式及全局主题 `theme`。
*   复杂的宏定义与内嵌 HTML/Markdown。

### 4. 按行解析的 Parser 设计
第一版采用**按行扫描（Line-by-Line Scan）**即可：
*   单行扫描识别 `participant` / `message` / `note`。
*   遇到多行 `note` 时进入块读取模式，直到匹配到结束符。
*   遇到嵌套控制块时使用**栈（Stack）**结构进行嵌套层级管理。
*   不急于使用复杂的 LALR/LL 语法生成器，以求逻辑直观。

### 5. 架构解耦设计
不要让 `QGraphicsScene` 或 `QGraphicsItem` 直接面对和解析 PlantUML 文本。
*   **Parser**：只负责将文本转为 AST。
*   **LayoutGraph**：把 AST 转换为通用图结构，表达节点、边、rank、方向和 label。
*   **DotBuilder / dot -Tjson**：把 LayoutGraph 交给 Graphviz 自动排版，取得节点坐标、边路径和文本位置。
*   **RenderModel**：独立于 Qt，保存纯绘制数据模型（RenderDocument），包括 QRectF、QPainterPath、文本、样式和源码行号。
*   **QGraphicsScene / Renderer**：读取 RenderDocument，只负责图元的组装、双层卡片绘制与交互响应。

### 6. 性能目标
通过 **C++ 内存解析 + QGraphicsScene 局部矢量刷新**，实现比原生 Java 调用 `plantuml.jar` 渲染快数个数量级的响应速度，做到打字即渲染的“零延迟”无极缩放效果。

---

## 最短总结
*   先看官方文档定规则。
*   再看 PlantUML 源码查细节。
*   然后只实现自己的 PlantUML 子集。
*   第一版优先做时序图。
*   解析生成 AST，转换为 LayoutGraph，Graphviz 排版生成 RenderModel，最后用 QGraphicsScene 渲染。
*   **核心原则：源码是参考，子集规范才是你的开发依据。**

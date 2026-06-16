# Implement QGraphicsScene and QGraphicsView Version Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 抛弃 QML 界面，将项目重构为纯 Qt Widgets 架构，利用 `QGraphicsView` + `QGraphicsScene` 对 PlantUML 渲染的 SVG 进行高性能矢量级展示，并提供双栏编辑及以鼠标为中心的无极缩放与平移。

**Architecture:** 
1. **左侧**：`QPlainTextEdit` 源码编辑器，连接 800ms 防抖定时器。
2. **右侧**：自定义 `PlantUmlView` (继承 `QGraphicsView`)，用于加载 `QGraphicsSvgItem` 并拦截 Ctrl+滚轮 进行以鼠标为中心的缩放。
3. **网络层**：使用 `QNetworkAccessManager` 发送在线请求，接收到 SVG 数据直接通过内存流 `QSvgRenderer` 渲染在场景中。
4. **整体排版**：使用 `QSplitter` 进行双栏切分，整体采用 QSS (Qt Style Sheets) 实现暗黑美化。

**Tech Stack:** Qt6 (Widgets, Network, Svg, SvgWidgets), C++17

---

### Task 1: 修改 CMake 依赖，引入 Widgets/Network/Svg 模块

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/CMakeLists.txt`

**Step 1: 重写 CMakeLists.txt 依赖**
寻找 `Widgets`, `Network`, `Svg`, `SvgWidgets` 依赖，移除所有 QML 相关的 module 宏，重新定义源文件编译结构。

---

### Task 2: 重新规划 C++ 目录与类结构

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/plantuml_view.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/plantuml_view.cpp`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/mainwindow.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/mainwindow.cpp`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/main.cpp`
- Remove: `F:/B_My_Document/GitHub/plantuml-qt/src/Main.qml` (清理无用文件)
- Remove: `F:/B_My_Document/GitHub/plantuml-qt/src/Theme.qml` (清理无用文件)
- Remove: `F:/B_My_Document/GitHub/plantuml-qt/src/TypeScale.qml` (清理无用文件)

**Step 1: 编写 plantuml_view (.h/.cpp)**
继承 `QGraphicsView`，实现 `wheelEvent` 以鼠标为中心缩放，设定 `ScrollHandDrag` 拖拽平移模式。

**Step 2: 编写 mainwindow (.h/.cpp)**
内置 `QSplitter` 双栏排版。左侧为 `QPlainTextEdit`，右侧为自定义的 `PlantUmlView`。配置 800ms `QTimer` 延迟渲染和 `QNetworkAccessManager` 请求 SVG 数据并渲染。使用 QSS 设定高质感暗黑主题。

**Step 3: 修改 main.cpp 启动逻辑**
移除所有 QQml 引擎载入，改为实例化并展示 `MainWindow`。

---

### Task 3: 清理缓存、独立目录编译与运行测试

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/build/`

**Step 1: 重新配置 CMake 并编译**
清空旧的 `build` 文件夹。运行 MinGW 配置并重新构建。
Run: `if (Test-Path build) { Remove-Item -Recurse -Force build }; cmake -G \"MinGW Makefiles\" -B build -S .; cmake --build build`
Expected: 编译完全无报错，生成 `plantuml-qt.exe`。

**Step 2: 运行测试**
验证桌面窗口拉起正常，双栏布局完美，可流畅地编辑和按 Ctrl+滚轮 缩放 PlantUML 图像。

---

### Task 4: 将新代码同步到 GitHub

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/`

**Step 1: 运行 Git 命令同步**
`git add .`
`git commit -m "feat: rewrite project to use QGraphicsScene/QGraphicsView for premium vector zoom/pan rendering"`
`git push`

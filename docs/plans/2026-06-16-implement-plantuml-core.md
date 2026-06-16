# Implement PlantUML Core Renderer and Split UI Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 实现 C++ 端 PlantUML 压缩编码逻辑，并搭建 QML 双栏交互界面（支持源码编辑与极速无极缩放预览）。

**Architecture:** C++ 编写 `PlantUmlEncoder` 逻辑，在其中剥离 `qCompress` 头部的 4 字节数据，再转码成官方变体 Base64 并注册入 QML 模块。QML 端通过 `SplitView` 进行左右排版，右边由 `Flickable` 提供 Pan&Zoom 渲染功能。

**Tech Stack:** Qt6 (QML SplitView, Flickable), C++17

---

### Task 1: 编写 PlantUML 文本压缩编码器

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/plantuml_encoder.h`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/plantuml_encoder.cpp`

**Step 1: 编写 plantuml_encoder.h**
声明继承自 `QObject` 的 `PlantUmlEncoder` 类，暴露 `Q_INVOKABLE QString encode(const QString &text)`。

**Step 2: 编写 plantuml_encoder.cpp**
实现编码算法：将文本转为 UTF-8 字节流，使用 `qCompress` 压缩，跳过前 4 字节的 Qt 私有头部，使用 PlantUML 64 位字符表进行 3-to-4 转换。

---

### Task 2: 将 C++ 编码器注册至 QML 模块中

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/CMakeLists.txt`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/main.cpp`

**Step 1: 在 CMakeLists.txt 中注册 C++ 类**
将 `src/plantuml_encoder.h` 和 `src/plantuml_encoder.cpp` 加入可执行程序源文件列表，并让 `qt_add_qml_module` 识别。

**Step 2: 在 main.cpp 中注册到 QML 引擎**
使用 `qmlRegisterType` 或 `qmlRegisterSingletonInstance`，或者通过 `QML_ELEMENT` 在 C++ 中自动暴露。在 Qt6 中，我们直接在类中添加 `QML_ELEMENT` 宏即可。

---

### Task 3: 改造 UI 实现左右双栏交互

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/Main.qml`

**Step 1: 替换主区域为 SplitView**
加入左侧 `TextArea`（文本编辑区）和右侧 `Flickable`（预览区）。

**Step 2: 在预览区实现拖拽平移与无极缩放**
利用 `MouseArea` 获取滚轮事件与拖拽，调整图片的 `scale` 或宽高，实现极度流畅的 Pan & Zoom。

---

### Task 4: 编译运行并同步代码

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/build/`

**Step 1: 重新编译并执行**
在根目录重新运行 `cmake --build build`，验证无编译错误。
在 Windows 上启动该程序测试。

**Step 2: 将改动推送至 GitHub**
Run: `git add . && git commit -m "feat: implement plantuml core renderer and split layout with zoom"`
Run: `git push`

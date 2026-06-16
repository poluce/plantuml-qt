# Initialize plantuml-qt Project Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 初始化 `plantuml-qt` 项目，配置基于 CMake 和 Qt6 QML 的极简、高性能架构，并将其关联推送到 GitHub 公开仓库。

**Architecture:** 采用 Qt6 官方推荐的 CMake 与 QML 模块化管理方式。主界面基于 QML Quick Controls，使用 QML 单例管理主题配色和排版字体，使用独立 build 目录进行编译，保证代码目录整洁。

**Tech Stack:** C++17, Qt6 (Quick, Qml, Controls), CMake, Git, GitHub CLI (gh)

---

### Task 1: 创建项目目录与基础 Git 文件

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/.gitignore`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/README.md`

**Step 1: 创建 .gitignore**
写入适用于 Qt6 编译隔离和常用编辑器的忽略配置。

**Step 2: 创建 README.md**
写入项目简介和编译安装说明。

**Step 3: 初始化 Git 仓库**
在项目根目录运行 `git init` 并提交初始文件。
Run: `git init && git add . && git commit -m "chore: initial commit with readme and gitignore"`

---

### Task 2: 创建 GitHub 远程仓库并推送

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/README.md`

**Step 1: 使用 gh cli 创建公开仓库**
Run: `gh repo create plantuml-qt --public --source=. --remote=origin --push`
Expected: 远程仓库创建成功，并且将本地代码推送到 GitHub 的 `main` 或 `master` 分支。

---

### Task 3: 配置 CMakeLists.txt

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/CMakeLists.txt`

**Step 1: 编写 CMakeLists.txt 基础配置**
引入 Qt6 核心及 Quick/Qml 模块，添加 QML 模块化定义，保证编译时能够正确识别 QML 文件。

---

### Task 4: 编写 C++ 入口与主 QML 窗口

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/main.cpp`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/main.qml`

**Step 1: 编写 main.cpp**
初始化 `QGuiApplication`，并加载 QML 引擎，载入 `main.qml`。

**Step 2: 编写 main.qml**
使用 QtQuick.Controls 创建一个基础的窗口，提供现代暗黑主题背景和一个简单的 "Hello PlantUML Viewer" 文本。

---

### Task 5: 引入 UI 设计与主题单例 (Theme & TypeScale)

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/Theme.qml`
- Create: `F:/B_My_Document/GitHub/plantuml-qt/src/TypeScale.qml`
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/CMakeLists.txt`

**Step 1: 创建 Theme.qml**
声明暗黑主题色板（如背景色、文字颜色、交互主色调等）。

**Step 2: 创建 TypeScale.qml**
遵循 `qt-ui-design` 规范中的 Perfect Fourth 比例，实现跨平台字体大小适配单例。

**Step 3: 注册 QML 模块依赖**
更新 `CMakeLists.txt` 以引入这两个单例。

---

### Task 6: 独立目录编译与运行测试

**Files:**
- Create: `F:/B_My_Document/GitHub/plantuml-qt/build/`

**Step 1: 在独立的 build 目录下构建**
在根目录下运行：
Run: `mkdir build; cd build; cmake ..; cmake --build .`
Expected: 编译无报错，生成 `plantuml-qt` 可执行程序。

**Step 2: 运行可执行程序测试**
运行生成的目标程序，验证窗口正常弹起，QML 界面渲染正常。

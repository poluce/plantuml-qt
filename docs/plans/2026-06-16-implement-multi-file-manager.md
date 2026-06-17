# Implement Multi-File Project Manager Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 实现多文件管理切换面板，将主界面升级为三栏布局（左：项目列表与打开按钮，中：代码编辑，右：图形预览），支持自由导入 `.puml` 文件、自动保存内存编辑草稿以及一键切换查看。

**Architecture:** 
1. **数据管理**：在 `MainWindow` 中声明 `OpenedFile` 结构体，使用 `QHash<QListWidgetItem*, OpenedFile>` 将列表中的 UI 项与文件内存数据完美关联。
2. **界面重组**：使用 `QSplitter` 横向塞入三个大栏（左侧管理面板、中间编辑器、右侧视口），并使用 QSS 对列表控件进行现代扁平高亮美化。
3. **切换机制**：重写 `onCurrentFileChanged(current, previous)`，自动暂存 previous 项目的修改，切换 current 的文本并触发 `RenderController` 本地重绘。

---

### Task 1: 升级 MainWindow 结构，加入多文件管理体系

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/mainwindow.h`

**Step 1: 增加 OpenedFile 结构体与成员定义**
定义 `OpenedFile` 保存路径和内容，并在类中定义 `QListWidget*`、`QPushButton*` 以及 `QHash<QListWidgetItem*, OpenedFile> m_files` 关联表。

**Step 2: 声明相关的槽函数**
声明 `onOpenFileClicked()` 处理文件对话框读取；声明 `onCurrentFileChanged(QListWidgetItem*, QListWidgetItem*)` 处理列表切换。

---

### Task 2: 实现三栏布局组装与 QSS 美化

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/mainwindow.cpp`

**Step 1: 编写 setupUi() 三栏逻辑**
创建左侧管理面板容器 `QWidget`，垂直排列“打开文件”按钮与 `QListWidget`。将该容器、编辑器与视口共同塞入 `splitter` 成为三栏。

**Step 2: 在 setupStyles() 中美化 QListWidget**
注入符合现代浅色主题的样式表：为列表项添加圆角、内边距，悬停为淡灰，选中时高亮为浅蓝色并变粗，字色为靛蓝。

---

### Task 3: 编写文件导入、草稿保存与切换逻辑

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/src/mainwindow.cpp`

**Step 1: 实现 onOpenFileClicked()**
调用 `QFileDialog::getOpenFileName`。读取文件失败则报错；若读取成功，先查重（若已打开直接切过去），否则新建 Item 插入列表，并自动选中。

**Step 2: 实现 onCurrentFileChanged()**
保存旧文件编辑草稿，载入新文件内存文本，刷新状态栏和标题栏，并向 `renderController` 提交新文本进行瞬时局部重绘。

**Step 3: 构造函数默认加载一个初始项目**
使程序启动时列表自动存在一个 `untitled.puml`，填入默认时序图图例，提升开箱即用的体验。

---

### Task 4: 编译运行与 Git 同步

**Files:**
- Modify: `F:/B_My_Document/GitHub/plantuml-qt/build/`

**Step 1: 编译测试**
运行 `cmake --build build` 确保无编译报错。
打开 `.exe` 文件测试三栏拉伸、文件选择打开和列表切换。

**Step 2: 推送代码**
`git add .`
`git commit -m "feat: implement multi-file manager sidebar with modern light QListWidget theme"`
`git push`

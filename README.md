# plantuml-qt

基于 Qt6 和 QML 的高性能 PlantUML 桌面查看器。

## 功能特性
- **极速查看**：通过在线/本地异步渲染，规避 VS Code 等插件在重度使用时的卡顿感。
- **流畅交互**：支持丝滑的鼠标平移、滚轮缩放、手势捏合（基于 QML Flickable + PinchArea）。
- **精致 UI**：根据 Qt UI 最佳实践（暗黑主题风格，无缝界面缩放）。

## 开发与构建

项目采用 out-of-source 编译，请在独立的 `build` 文件夹中进行编译，禁止在项目根目录下直接编译。

### 编译步骤
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

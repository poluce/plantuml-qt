cmake_minimum_required(VERSION 3.16)

project(plantuml-qt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 AUTOMOC
set(CMAKE_AUTOMOC ON)

# 寻找 Qt6 依赖 (仅需最底层的 Widgets 与 Network，Svg/SvgWidgets 保留用作后期导出)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg SvgWidgets)

# 定义项目的所有源码文件
qt_add_executable(plantuml-qt
    # 程序入口与控制主窗口
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/app/RenderController.h
    src/app/RenderController.cpp

    # 词法/语法解析器
    src/parser/SourceLocation.h
    src/parser/ParseError.h
    src/parser/Token.h
    src/parser/PumlLexer.h
    src/parser/PumlLexer.cpp
    src/parser/PumlParser.h
    src/parser/PumlParser.cpp
    src/parser/PumlHighlighter.h
    src/parser/PumlHighlighter.cpp

    # AST 语义树
    src/ast/DiagramAst.h

    # 渲染无关的几何图元模型
    src/render_model/RenderDocument.h
    src/render_model/RenderTheme.h

    # Graphviz 布局管线
    src/layout_graph/LayoutGraph.h
    src/graphviz/GraphLayoutResult.h
    src/graphviz/DotBuilder.h
    src/graphviz/DotBuilder.cpp
    src/graphviz/DotProcessRunner.h
    src/graphviz/DotProcessRunner.cpp
    src/graphviz/DotJsonParser.h
    src/graphviz/DotJsonParser.cpp
    src/graphviz/GraphvizLayoutEngine.h
    src/graphviz/GraphvizLayoutEngine.cpp

    # 画布视口交互层
    src/graphics/DiagramScene.h
    src/graphics/DiagramScene.cpp
    src/graphics/DiagramSceneRenderer.h
    src/graphics/DiagramSceneRenderer.cpp
    src/graphics/DiagramView.h
    src/graphics/DiagramView.cpp

    # 自定义高度内聚图元项
    src/graphics/items/ParticipantItem.h
    src/graphics/items/ParticipantItem.cpp
    src/graphics/items/MessageArrowItem.h
    src/graphics/items/MessageArrowItem.cpp
    src/graphics/items/ClassBoxItem.h
    src/graphics/items/ClassBoxItem.cpp
    src/graphics/items/RelationItem.h
    src/graphics/items/RelationItem.cpp
    src/graphics/items/NoteLineItem.h
    src/graphics/items/NoteLineItem.cpp
    src/graphics/items/PackageGroupItem.h
    src/graphics/items/PackageGroupItem.cpp
)

# 链接 Qt 库
target_link_libraries(plantuml-qt
    PRIVATE 
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
        Qt6::SvgWidgets
)

# 添加解析器测试目标
add_executable(test_parser
    tests/test_parser.cpp
    src/parser/SourceLocation.h
    src/parser/ParseError.h
    src/parser/PumlParser.h
    src/parser/PumlParser.cpp
    src/ast/DiagramAst.h
    src/graphviz/DotBuilder.h
    src/graphviz/DotBuilder.cpp
    src/graphviz/GraphvizLayoutEngine.h
    src/graphviz/GraphvizLayoutEngine.cpp
    src/graphviz/DotJsonParser.h
    src/graphviz/DotJsonParser.cpp
    src/graphviz/DotProcessRunner.h
    src/graphviz/DotProcessRunner.cpp
)
target_link_libraries(test_parser PRIVATE Qt6::Widgets)


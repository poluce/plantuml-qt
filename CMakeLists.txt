cmake_minimum_required(VERSION 3.16)

project(plantuml-qt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options(/permissive-)
    add_compile_options(/utf-8)
endif()

# 启用 AUTOMOC
set(CMAKE_AUTOMOC ON)

# 寻找 Qt6 依赖
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg SvgWidgets)

# 定义底层的核心编译静态库 (包含词法、语法解析器以及 Graphviz 布局引擎)
add_library(plantuml-core STATIC
    src/parser/SourceLocation.h
    src/parser/ParseError.h
    src/parser/Token.h
    src/parser/PumlLexer.h
    src/parser/PumlLexer.cpp
    src/parser/PumlParser.h
    src/parser/PumlParser.cpp
    src/parser/PumlHighlighter.h
    src/parser/PumlHighlighter.cpp
    
    src/ast/DiagramAst.h
    src/render_model/RenderDocument.h
    src/render_model/RenderTheme.h
    
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
)

target_link_libraries(plantuml-core PUBLIC
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::SvgWidgets
)

# 主程序可执行目标
qt_add_executable(plantuml-qt
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/app/RenderController.h
    src/app/RenderController.cpp
    src/app/CliProcessor.h
    src/app/CliProcessor.cpp

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

target_link_libraries(plantuml-qt PRIVATE plantuml-core)

# 单元测试可执行目标
qt_add_executable(test_parser
    tests/test_parser.cpp
)

target_link_libraries(test_parser PRIVATE
    plantuml-core
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::SvgWidgets
)

enable_testing()
add_test(NAME test_parser COMMAND test_parser)

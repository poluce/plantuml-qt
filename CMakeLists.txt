cmake_minimum_required(VERSION 3.16)

project(plantuml-qt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 AUTOMOC (Qt 槽函数与信号所必须)
set(CMAKE_AUTOMOC ON)

# 寻找 Qt6 依赖，引入 Widgets、Network、Svg 和 SvgWidgets (QGraphicsSvgItem)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg SvgWidgets)

# 定义项目的所有 C++ 头文件与源文件
qt_add_executable(plantuml-qt
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/plantuml_view.h
    src/plantuml_view.cpp
    src/plantuml_encoder.h
    src/plantuml_encoder.cpp
)

# 链接 Qt 核心 Widgets、Network、Svg 以及 SvgWidgets 模块
target_link_libraries(plantuml-qt
    PRIVATE 
        Qt6::Widgets 
        Qt6::Network 
        Qt6::Svg 
        Qt6::SvgWidgets
)

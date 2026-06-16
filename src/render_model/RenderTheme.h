#pragma once

#include <QColor>

struct RenderTheme
{
    // 主题配色 (默认使用精致的现代浅色方案)
    QColor backgroundColor = QColor("#f9fafb");
    QColor surfaceColor = QColor("#ffffff");
    QColor onSurfaceColor = QColor("#18181b");
    QColor onSurfaceMutedColor = QColor("#71717a");
    
    QColor primaryColor = QColor("#6366f1");      // 主色调: 靛蓝 (Indigo 500)
    QColor primaryLightColor = QColor("#e0e7ff"); // 主色调浅背景: 浅靛蓝 (Indigo 100)
    QColor borderColor = QColor("#e5e7eb");       // 细线边框色 (Gray 200)
    QColor errorColor = QColor("#ef4444");         // 错误标识色
    
    double nodeRadius = 6.0;
    double lineWidth = 1.5;
    double arrowSize = 8.0;
};

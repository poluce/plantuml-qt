#pragma once

struct SourceLocation
{
    int line = -1;      // 1-indexed 行号
    int column = -1;    // 1-indexed 列号
    int length = 0;     // 标识符/文本长度
};

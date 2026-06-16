#pragma once

#include <QString>
#include "SourceLocation.h"

enum class ParseErrorSeverity
{
    Warning,
    Error,
    Fatal
};

struct ParseError
{
    SourceLocation location;
    QString message;
    ParseErrorSeverity severity = ParseErrorSeverity::Error;
};

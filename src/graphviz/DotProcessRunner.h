#pragma once

#include <QByteArray>
#include <QString>

struct DotProcessResult
{
    QByteArray stdoutData;
    QString stderrText;
    int exitCode = -1;
    bool timedOut = false;
};

class DotProcessRunner
{
public:
    DotProcessResult runJson(const QString &dotSource) const;
};

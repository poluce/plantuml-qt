#include "DotProcessRunner.h"
#include <QDebug>
#include <QProcess>

DotProcessResult DotProcessRunner::runJson(const QString &dotSource) const
{
    DotProcessResult result;
    QProcess process;
    qDebug() << "[Graphviz] 启动 dot -Tjson，DOT 字符数:" << dotSource.size();

    process.start("dot", {"-Tjson"});
    if (!process.waitForStarted(3000)) {
        result.stderrText = "无法启动 Graphviz dot，请确认 dot 已加入 PATH。";
        qWarning() << "[Graphviz]" << result.stderrText;
        return result;
    }

    process.write(dotSource.toUtf8());
    process.closeWriteChannel();
    if (!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished();
        result.timedOut = true;
        result.stderrText = "Graphviz dot 排版超时。";
        qWarning() << "[Graphviz]" << result.stderrText;
        return result;
    }

    result.exitCode = process.exitCode();
    result.stdoutData = process.readAllStandardOutput();
    result.stderrText = QString::fromUtf8(process.readAllStandardError());
    if (result.exitCode != 0) {
        qWarning() << "[Graphviz] dot 退出码:" << result.exitCode << "stderr:" << result.stderrText;
    } else if (!result.stderrText.trimmed().isEmpty()) {
        qWarning() << "[Graphviz] dot warning:" << result.stderrText.trimmed();
    } else {
        qDebug() << "[Graphviz] dot 完成，JSON 字节数:" << result.stdoutData.size();
    }
    return result;
}

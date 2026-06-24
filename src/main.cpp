#include "mainwindow.h"
#include "app/CliProcessor.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // 如果存在命令行参数，委派给专职的 CLI 自由函数执行，实现零耦合
    if (argc > 1) {
        return cli::run(argc, argv);
    }

    // 启用高 DPI 的半像素自适应对齐，防止界面在 Windows 缩放时模糊
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setOrganizationName("Poluce");
    app.setApplicationName("plantuml-qt");

    // 创建并展示主窗口
    MainWindow w;
    w.show();

    return app.exec();
}

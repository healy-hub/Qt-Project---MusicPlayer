#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QDir>

/** @brief 程序入口：创建 QApplication、设置窗口图标、显示主窗口并进入事件循环。 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 如果程序在独立环境下运行（如 /opt/MusicPlayer），手动设置插件库路径
    // 假设结构为：bin/MusicPlayer, plugins/platforms/...
    QString appDirPath = QCoreApplication::applicationDirPath();
    QDir appDir(appDirPath);
    appDir.cdUp(); // 进入安装根目录
    if (appDir.exists("plugins")) {
        QCoreApplication::addLibraryPath(appDir.absoluteFilePath("plugins"));
    }

    a.setWindowIcon(QIcon(":/res/misaka.png"));

    MainWindow w;
    w.show();
    return a.exec();
}

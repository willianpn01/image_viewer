#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("CImgViewer");
    app.setOrganizationName("CImgViewer");

    MainWindow w;
    w.show();

    // Open file passed on the command line
    if (argc > 1) w.openPath(QString::fromLocal8Bit(argv[1]));

    return app.exec();
}

#include <QApplication>
#include <QTranslator>
#include <QSettings>
#include <QLibraryInfo>
#include "MainWindow.hpp"
#include "Logger.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("CImgViewer");
    app.setOrganizationName("CImgViewer");

    // Load language from settings (default: pt_BR)
    QSettings settings("CImgViewer", "CImgViewer");
    QString lang = settings.value("language", "pt_BR").toString();

    QTranslator translator;
    QString qmPath = QString(":/translations/cimg-viewer_%1.qm").arg(lang);
    LOG_INFO("I18n", "Idioma configurado: " + lang);
    LOG_INFO("I18n", "Tentando carregar: " + qmPath);
    bool ok = translator.load(qmPath);
    if (ok) {
        app.installTranslator(&translator);
        LOG_INFO("I18n", "Translator carregado com sucesso");
    } else {
        LOG_WARNING("I18n", "FALHA ao carregar .qm: " + qmPath);
    }

    MainWindow w;
    w.show();

    // Open file passed on the command line
    if (argc > 1) w.openPath(QString::fromLocal8Bit(argv[1]));

    return app.exec();
}

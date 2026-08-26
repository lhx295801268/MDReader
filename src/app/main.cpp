#include <QApplication>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setOrganizationName("MDReader");
    QApplication::setApplicationName("MDReader");
    QApplication::setApplicationVersion("0.1.0");

    QWidget window;
    window.setWindowTitle("MDReader");
    window.resize(800, 600);
    window.show();

    return app.exec();
}
#include <QApplication>
#include "PlayerDialog.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    PlayerDialog dialog;
    dialog.show();
    
    return app.exec();
}
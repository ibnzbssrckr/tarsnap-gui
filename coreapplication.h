#ifndef COREAPPLICATION_H
#define COREAPPLICATION_H

#include "mainwindow.h"
#include "jobmanager.h"

#include <QObject>
#include <QApplication>
#include <QSettings>

class CoreApplication : public QApplication
{
    Q_OBJECT

public:
    CoreApplication(int &argc, char **argv);
    ~CoreApplication();

    void quitApplication(int returnCode);

private:
    MainWindow *_mainWindow;
    QSettings   _settings;
    JobManager  _jobManager;
};

#endif // COREAPPLICATION_H
#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <taskmanager.h>

#include <QDialog>
#include <QMouseEvent>
#include <QMovie>

namespace Ui {
class SetupDialog;
}

class SetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SetupDialog(QWidget *parent = 0);
    ~SetupDialog();

public slots:
    void wizardPageChanged(int);
    void skipToPage();
    void setNextPage();
    void showTarsnapPathBrowse();
    void showTarsnapCacheBrowse();
    void showAppDataBrowse();
    bool validateAdvancedSetupPage();
    void restoreNo();
    void restoreYes();
    void validateRegisterPage();
    void registerHaveKeyBrowse(QString url);
    void registerMachine();
    void commitSettings(bool skipped = false);

    // TaskManager responses
    void registerMachineStatus(TaskStatus status, QString reason);
    void updateLoadingAnimation(bool idle);

signals:
    void registerMachine(QString user, QString password, QString machine
                         ,QString key, QString tarsnapPath, QString cachePath);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

private:
    Ui::SetupDialog *_ui;
    QPoint           _windowDragPos;
    QMovie           _loadingAnimation;

    QString _tarsnapCLIDir;
//    QString _tarsnapUser;
//    QString _tarsnapPassword;
    QString _tarsnapCacheDir;
    QString _appDataDir;
    QString _tarsnapKeyFile;
    bool    _haveKey;
};

#endif // SETUPDIALOG_H

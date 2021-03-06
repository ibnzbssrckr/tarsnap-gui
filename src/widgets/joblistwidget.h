#ifndef JOBLISTWIDGET_H
#define JOBLISTWIDGET_H

#include "persistentmodel/job.h"
#include "taskmanager.h"

#include <QListWidget>

class JobListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit JobListWidget(QWidget *parent = 0);
    ~JobListWidget();

signals:
    void displayJobDetails(JobPtr job);
    void backupJob(BackupTaskPtr backup);
    void restoreArchive(ArchivePtr archive, ArchiveRestoreOptions options);
    void deleteJob(JobPtr job, bool purgeArchives);

public slots:
    void addJobs(QMap<QString, JobPtr> jobs);
    void backupSelectedItems();
    void selectJob(JobPtr job);
    void selectJobByRef(QString jobRef);
    void backupAllJobs();

private slots:
    void backupItem();
    void inspectItem();
    void restoreItem();
    void deleteItem();
    void addJob(JobPtr job);

protected:
    void keyReleaseEvent(QKeyEvent *event);
};

#endif // JOBLISTWIDGET_H

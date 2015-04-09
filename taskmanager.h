#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "tarsnapclient.h"
#include "persistentmodel/archive.h"

#include <QObject>
#include <QThread>
#include <QUrl>
#include <QUuid>
#include <QMap>
#include <QSharedPointer>
#include <QDateTime>
#include <QThreadPool>
#include <QQueue>


#define CMD_TARSNAP         "tarsnap"
#define CMD_TARSNAPKEYGEN   "tarsnap-keygen"

struct ArchiveRestoreOptions
{
    bool        preservePaths;
    bool        overwriteFiles;
    bool        keepNewerFiles;
    QString     chdir;
};

enum TaskStatus { Queued, Running, Completed, Failed, Paused, Initialized };

class BackupTask
{
public:
    QUuid                 uuid;
    QList<QUrl>           urls;
    QString               name;
    TaskStatus            status;
    int                   exitCode;
    QString               output;
    ArchivePtr            archive;

    BackupTask():uuid(QUuid::createUuid()),status(TaskStatus::Initialized){}
};

typedef QSharedPointer<BackupTask> BackupTaskPtr;

class TaskManager : public QObject
{
    Q_OBJECT

public:
    explicit TaskManager(QObject *parent = 0);
    ~TaskManager();

signals:
    void idle(bool status); // signal if we are working on tasks or not
    void registerMachineStatus(TaskStatus status, QString reason);
    void fsckStatus(TaskStatus status, QString reason);
    void nukeStatus(TaskStatus status, QString reason);
    void backupTaskUpdate(BackupTaskPtr backupTask);
    void archivesList(QList<ArchivePtr> archives);
    void archivesDeleted(QList<ArchivePtr> archives);
    void overallStats(qint64 sizeTotal, qint64 sizeCompressed, qint64 sizeUniqueTotal
                      , qint64 sizeUniqueCompressed, qint64 archiveCount, qreal credit
                      , QString accountStatus);
    void restoreArchiveStatus(ArchivePtr archive, TaskStatus status, QString reason);

public slots:
    void loadSettings();

    void registerMachine(QString user, QString password, QString machine
                         ,QString key, QString tarsnapPath, QString cachePath);
    void backupNow(BackupTaskPtr backupTask);
    void getArchivesList();
    void getArchiveStats(ArchivePtr archive);
    void getArchiveContents(ArchivePtr archive);
    void deleteArchives(QList<ArchivePtr> archives);
    void getOverallStats();
    void runFsck();
    void nukeArchives();
    void restoreArchive(ArchivePtr archive, ArchiveRestoreOptions options);

private slots:
    void backupTaskFinished(QUuid uuid, QVariant data, int exitCode, QString output);
    void backupTaskStarted(QUuid uuid);
    void registerMachineFinished(QUuid uuid, QVariant data, int exitCode, QString output);
    void getArchivesFinished(QUuid uuid, QVariant data, int exitCode, QString output);
    void getArchiveStatsFinished(QUuid uuid, QVariant data, int exitCode, QString output);
    void getArchiveContentsFinished(QUuid uuid, QVariant data, int exitCode, QString output);
    void deleteArchiveFinished(QUuid uuid, QVariant data, int exitCode, QString output);
    void overallStatsFinished(QUuid uuid, QVariant data, int exitCode, QString output);
    void fsckFinished(QUuid uuid, QVariant data, int exitCode, QString output);
    void nukeFinished(QUuid uuid, QVariant data, int exitCode, QString output);
    void restoreArchiveFinished(QUuid uuid, QVariant data, int exitCode, QString output);

    void queueTask(TarsnapClient *cli, bool exclusive = false);
    void startTask(TarsnapClient *cli);
    void dequeueTask(QUuid uuid, QVariant data, int exitCode, QString output);
    void parseArchiveStats(QString tarsnapOutput, bool newArchiveOutput, ArchivePtr archive);

private:
    QString makeTarsnapCommand(QString cmd);

private:
    QString                      _tarsnapDir;
    QString                      _tarsnapCacheDir;
    QString                      _tarsnapKeyFile;
    QThread                      _managerThread; // manager runs on a separate thread
    QMap<QUuid, BackupTaskPtr>   _backupTaskMap;
    QMap<QUuid, ArchivePtr>      _archiveMap;
    QMap<QUuid, TarsnapClient*>  _taskMap;
    QQueue<TarsnapClient*>       _taskQueue;
    QThreadPool                 *_threadPool;
    bool                         _aggressiveNetworking;
    bool                         _preservePathnames;
};

#endif // TASKMANAGER_H

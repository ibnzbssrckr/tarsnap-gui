#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_archiveitemwidget.h"
#include "ui_backupitemwidget.h"
#include "ui_aboutwidget.h"
#include "backuplistitem.h"
#include "filepickerdialog.h"
#include "utils.h"
#include "debug.h"

#include <QPainter>
#include <QSettings>
#include <QDateTime>
#include <QFileDialog>
#include <QDir>
#include <QSharedPointer>
#include <QHostInfo>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QInputDialog>
#include <QMenu>

#define PURGE_SECONDS_DELAY 8

MainWindow::MainWindow(QWidget *parent) :
    QWidget(parent),
    _ui(new Ui::MainWindow),
    _tarsnapLogo(this),
    _menuBar(NULL),
    _appMenu(NULL),
    _actionAbout(this),
    _useSIPrefixes(false),
    _purgeTimerCount(0),
    _purgeCountdownWindow(this)
{
    _ui->setupUi(this);

    _ui->backupListWidget->setAttribute(Qt::WA_MacShowFocusRect, false);
    _ui->archiveListWidget->setAttribute(Qt::WA_MacShowFocusRect, false);
    _ui->jobListWidget->setAttribute(Qt::WA_MacShowFocusRect, false);

    QPixmap logo(":/icons/tarsnap.png");
    _tarsnapLogo.setPixmap(logo.scaled(200, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    _tarsnapLogo.adjustSize();
    _tarsnapLogo.lower();
    _tarsnapLogo.show();

    Ui::aboutWidget aboutUi;
    aboutUi.setupUi(&_aboutWindow);
    aboutUi.versionLabel->setText(tr("version ") + QCoreApplication::applicationVersion());
    _aboutWindow.setWindowFlags(Qt::CustomizeWindowHint|Qt::WindowCloseButtonHint);
    connect(aboutUi.checkUpdateButton, &QPushButton::clicked,
            [=](){
                QDesktopServices::openUrl(QUrl("https://github.com/Tarsnap/tarsnap-gui/releases"));
            });

    if(_menuBar.isNativeMenuBar())
    {
        _actionAbout.setMenuRole(QAction::AboutRole);
        connect(&_actionAbout, SIGNAL(triggered()), &_aboutWindow, SLOT(show()));
        _appMenu.addAction(&_actionAbout);
        _menuBar.addMenu(&_appMenu);
    }
    connect(_ui->aboutButton, SIGNAL(clicked()), &_aboutWindow, SLOT(show()));

    _ui->mainContentSplitter->setCollapsible(0, false);
    _ui->journalLog->hide();
    _ui->archiveDetailsWidget->hide();
    _ui->jobDetailsWidget->hide();

    connect(&Debug::instance(), SIGNAL(message(QString)), this , SLOT(appendToConsoleLog(QString))
            , Qt::QueuedConnection);

    loadSettings();

    // Ui actions
    _ui->mainTabWidget->setCurrentWidget(_ui->backupTab);
    _ui->settingsToolbox->setCurrentWidget(_ui->settingsAccountPage);

    _ui->archiveListWidget->addAction(_ui->actionRefresh);
    connect(_ui->actionRefresh, SIGNAL(triggered()), this , SIGNAL(loadArchives()), Qt::QueuedConnection);
    _ui->backupListWidget->addAction(_ui->actionClearList);
    connect(_ui->actionClearList, SIGNAL(triggered()), _ui->backupListWidget
            , SLOT(clear()), Qt::QueuedConnection);
    _ui->backupListWidget->addAction(_ui->actionBrowseItems);
    connect(_ui->actionBrowseItems, SIGNAL(triggered()), this, SLOT(browseForBackupItems()));
    _ui->settingsTab->addAction(_ui->actionRefreshStats);
    connect(_ui->actionRefreshStats, SIGNAL(triggered()), this, SIGNAL(getOverallStats()));
    this->addAction(_ui->actionGoBackup);
    this->addAction(_ui->actionGoBrowse);
    this->addAction(_ui->actionGoJobs);
    this->addAction(_ui->actionGoSettings);
    this->addAction(_ui->actionGoHelp);
    this->addAction(_ui->actionShowJournal);
    connect(_ui->actionGoBackup, &QAction::triggered,
            [=](){
                _ui->mainTabWidget->setCurrentWidget(_ui->backupTab);
            });
    connect(_ui->actionGoBrowse, &QAction::triggered,
            [=](){
                _ui->mainTabWidget->setCurrentWidget(_ui->archivesTab);
            });
    connect(_ui->actionGoJobs, &QAction::triggered,
            [=](){
                _ui->mainTabWidget->setCurrentWidget(_ui->jobsTab);
            });
    connect(_ui->actionGoSettings, &QAction::triggered,
            [=](){
                _ui->mainTabWidget->setCurrentWidget(_ui->settingsTab);
            });
    connect(_ui->actionGoHelp, &QAction::triggered,
            [=](){
                _ui->mainTabWidget->setCurrentWidget(_ui->helpTab);
            });
    connect(_ui->actionShowJournal, SIGNAL(triggered()), _ui->expandJournalButton, SLOT(click()));
    connect(_ui->backupListInfoLabel, SIGNAL(linkActivated(QString)), this,
            SLOT(browseForBackupItems()));
    connect(_ui->backupButton, SIGNAL(clicked()), this, SLOT(backupButtonClicked()));
    connect(_ui->appendTimestampCheckBox, SIGNAL(toggled(bool)), this, SLOT(appendTimestampCheckBoxToggled(bool)));
    connect(_ui->accountMachineUseHostnameButton, SIGNAL(clicked()), this, SLOT(accountMachineUseHostnameButtonClicked()));
    connect(_ui->accountMachineKeyBrowseButton, SIGNAL(clicked()), this, SLOT(accountMachineKeyBrowseButtonClicked()));
    connect(_ui->tarsnapPathBrowseButton, SIGNAL(clicked()), this, SLOT(tarsnapPathBrowseButtonClicked()));
    connect(_ui->tarsnapCacheBrowseButton, SIGNAL(clicked()), this, SLOT(tarsnapCacheBrowseButton()));
    connect(_ui->repairCacheButton, SIGNAL(clicked()), this, SLOT(repairCacheButtonClicked()));
    connect(_ui->purgeArchivesButton, SIGNAL(clicked()), this, SLOT(purgeArchivesButtonClicked()));
    connect(_ui->runSetupWizard, SIGNAL(clicked()), this, SLOT(runSetupWizardClicked()));
    connect(_ui->expandJournalButton, SIGNAL(toggled(bool)), this, SLOT(expandJournalButtonToggled(bool)));
    connect(_ui->downloadsDirBrowseButton, SIGNAL(clicked()), this, SLOT(downloadsDirBrowseButtonClicked()));
    connect(_ui->busyWidget, SIGNAL(clicked()), this, SLOT(cancelRunningTasks()));
    connect(&_purgeTimer, SIGNAL(timeout()), this, SLOT(purgeTimerFired()));
    _purgeCountdownWindow.setIcon(QMessageBox::Critical);
    _purgeCountdownWindow.setWindowTitle(tr("Deleting all archives: press Cancel to abort"));
    _purgeCountdownWindow.setStandardButtons(QMessageBox::Cancel);

    // Settings page
    connect(_ui->accountUserLineEdit, SIGNAL(editingFinished()), this, SLOT(commitSettings()));
    connect(_ui->accountMachineLineEdit, SIGNAL(editingFinished()), this, SLOT(commitSettings()));
    connect(_ui->accountMachineKeyLineEdit, SIGNAL(editingFinished()), this, SLOT(commitSettings()));
    connect(_ui->tarsnapPathLineEdit, SIGNAL(editingFinished()), this, SLOT(commitSettings()));
    connect(_ui->tarsnapCacheLineEdit, SIGNAL(editingFinished()), this, SLOT(commitSettings()));
    connect(_ui->aggressiveNetworkingCheckBox, SIGNAL(toggled(bool)), this, SLOT(commitSettings()));
    connect(_ui->accountMachineKeyLineEdit, SIGNAL(textChanged(QString)), this, SLOT(validateMachineKeyPath()));
    connect(_ui->tarsnapPathLineEdit, SIGNAL(textChanged(QString)), this, SLOT(validateTarsnapPath()));
    connect(_ui->tarsnapCacheLineEdit, SIGNAL(textChanged(QString)), this, SLOT(validateTarsnapCache()));
    connect(_ui->siPrefixesCheckBox, SIGNAL(toggled(bool)), this, SLOT(commitSettings()));
    connect(_ui->preservePathsCheckBox, SIGNAL(toggled(bool)), this, SLOT(commitSettings()));
    connect(_ui->downloadsDirLineEdit, SIGNAL(editingFinished()), this, SLOT(commitSettings()));
    connect(_ui->traverseMountCheckBox, SIGNAL(toggled(bool)), this, SLOT(commitSettings()));
    connect(_ui->followSymLinksCheckBox, SIGNAL(toggled(bool)), this, SLOT(commitSettings()));
    connect(_ui->skipFilesSpinBox, SIGNAL(editingFinished()), this, SLOT(commitSettings()));

    // Backup and Archives
    connect(_ui->backupListWidget, SIGNAL(itemTotals(quint64,quint64)), this
            , SLOT(updateBackupItemTotals(quint64, quint64)));
    connect(this, SIGNAL(archiveList(QList<ArchivePtr >))
            , _ui->archiveListWidget, SLOT(addArchives(QList<ArchivePtr >)));
    connect(_ui->archiveListWidget, SIGNAL(inspectArchive(ArchivePtr)), this
            , SLOT(displayInspectArchive(ArchivePtr)));
    connect(_ui->archiveListWidget, SIGNAL(deleteArchives(QList<ArchivePtr>)), this
            , SIGNAL(deleteArchives(QList<ArchivePtr>)));
    connect(_ui->archiveListWidget, SIGNAL(restoreArchive(ArchivePtr,ArchiveRestoreOptions)),
            this, SIGNAL(restoreArchive(ArchivePtr,ArchiveRestoreOptions)));
    connect(_ui->archiveListWidget, SIGNAL(displayJobDetails(QString)),
            _ui->jobListWidget, SLOT(selectJobByRef(QString)));

    // Jobs
    connect(_ui->addJobButton, SIGNAL(clicked()), this, SLOT(addJobClicked()), Qt::QueuedConnection);
    connect(_ui->jobDetailsWidget, SIGNAL(cancel()), this, SLOT(hideJobDetails()), Qt::QueuedConnection);
    connect(_ui->jobDetailsWidget, SIGNAL(jobAdded(JobPtr)), _ui->jobListWidget, SLOT(addJob(JobPtr)), Qt::QueuedConnection);
    connect(_ui->jobDetailsWidget, SIGNAL(jobAdded(JobPtr)), _ui->jobListWidget, SLOT(selectJob(JobPtr)), Qt::QueuedConnection);
    connect(_ui->jobDetailsWidget, SIGNAL(inspectJobArchive(ArchivePtr)), this, SLOT(displayInspectArchive(ArchivePtr)), Qt::QueuedConnection);
    connect(_ui->jobDetailsWidget, SIGNAL(restoreJobArchive(ArchivePtr,ArchiveRestoreOptions)), this, SIGNAL(restoreArchive(ArchivePtr,ArchiveRestoreOptions)), Qt::QueuedConnection);
    connect(_ui->jobDetailsWidget, SIGNAL(deleteJobArchives(QList<ArchivePtr>)), this, SIGNAL(deleteArchives(QList<ArchivePtr>)), Qt::QueuedConnection);
    connect(_ui->jobDetailsWidget, SIGNAL(enableSave(bool)), _ui->addJobButton, SLOT(setEnabled(bool)), Qt::QueuedConnection);
    connect(_ui->jobListWidget, SIGNAL(displayJobDetails(JobPtr)), this, SLOT(displayJobDetails(JobPtr)), Qt::QueuedConnection);
    connect(_ui->jobListWidget, SIGNAL(backupJob(BackupTaskPtr)), this, SIGNAL(backupNow(BackupTaskPtr)), Qt::QueuedConnection);
    connect(_ui->jobListWidget, SIGNAL(restoreArchive(ArchivePtr,ArchiveRestoreOptions)), this, SIGNAL(restoreArchive(ArchivePtr,ArchiveRestoreOptions)), Qt::QueuedConnection);
    connect(_ui->jobListWidget, SIGNAL(deleteJob(JobPtr,bool)), this, SIGNAL(deleteJob(JobPtr,bool)), Qt::QueuedConnection);
    connect(this, SIGNAL(jobsList(QMap<QString,JobPtr>)), _ui->jobListWidget, SLOT(addJobs(QMap<QString,JobPtr>)), Qt::QueuedConnection);

    _ui->jobListWidget->addAction(_ui->actionAddJob);
    connect(_ui->actionAddJob, SIGNAL(triggered()), this, SLOT(addJobClicked()));
    QMenu *addJobMenu = new QMenu(_ui->addJobButton);
    addJobMenu->addAction(_ui->actionBackupAllJobs);
    connect(_ui->actionBackupAllJobs, SIGNAL(triggered()), _ui->jobListWidget, SLOT(backupAllJobs()));
    _ui->addJobButton->setMenu(addJobMenu);
    _ui->jobListWidget->addAction(_ui->actionJobBackup);
    connect(_ui->actionJobBackup, SIGNAL(triggered()), _ui->jobListWidget, SLOT(backupSelectedItems()));

    //lambda slots to quickly update various UI components
    connect(this, &MainWindow::loadArchives,
            [=](){updateStatusMessage(tr("Updating archives list from remote..."));});
    connect(this, &MainWindow::archiveList,
            [=](const QList<ArchivePtr> archives, bool fromRemote){
                Q_UNUSED(archives);
                if(fromRemote)
                    updateStatusMessage(tr("Updating archives list from remote...done"));
            });
    connect(this, &MainWindow::loadArchiveStats,
            [=](const ArchivePtr archive){updateStatusMessage(tr("Fetching details for archive <i>%1</i>.").arg(archive->name()));});
    connect(this, &MainWindow::loadArchiveContents,
            [=](const ArchivePtr archive){updateStatusMessage(tr("Fetching contents for archive <i>%1</i>.").arg(archive->name()));});
    connect(_ui->archiveListWidget, &ArchiveListWidget::deleteArchives,
            [=](const QList<ArchivePtr> archives){archivesDeleted(archives,false);});
    connect(_ui->backupNameLineEdit, &QLineEdit::textChanged,
            [=](const QString text){
                if(text.isEmpty())
                {
                    _ui->backupButton->setEnabled(false);
                    _ui->appendTimestampCheckBox->setChecked(false);
                }
                else if(_ui->backupListWidget->count())
                {
                    _ui->backupButton->setEnabled(true);
                }
            });
    connect(this, &MainWindow::restoreArchive,
            [=](const ArchivePtr archive){updateStatusMessage(tr("Restoring archive <i>%1</i>...").arg(archive->name()));});
    connect(_ui->downloadsDirLineEdit, &QLineEdit::textChanged,
            [=](){
                QFileInfo file(_ui->downloadsDirLineEdit->text());
                if(file.exists() && file.isDir() && file.isWritable())
                    _ui->downloadsDirLineEdit->setStyleSheet("QLineEdit{color:black;}");
                else
                    _ui->downloadsDirLineEdit->setStyleSheet("QLineEdit{color:red;}");
            });
    connect(_ui->jobListWidget, &JobListWidget::backupJob,
            [=](BackupTaskPtr backup){
                connect(backup, SIGNAL(statusUpdate(const TaskStatus&)), this, SLOT(backupTaskUpdate(const TaskStatus&)), Qt::QueuedConnection);
            });
    connect(_ui->jobListWidget, &JobListWidget::deleteJob,
            [=](JobPtr job, bool purgeArchives){
                if(purgeArchives)
                {
                    updateStatusMessage(tr("Job <i>%1</i> deleted. Deleting %2 associated archives next...").arg(job->name()).arg(job->archives().count()));
                }
                else
                {
                    updateStatusMessage(tr("Job <i>%1</i> deleted.").arg(job->name()));
                }
            });
    connect(_ui->jobDetailsWidget, &JobWidget::jobAdded,
            [=](JobPtr job){
                updateStatusMessage(tr("Job <i>%1</i> added.").arg(job->name()));
            });
    connect(_ui->loginTarsnapButton, &QPushButton::clicked,
            [=](){
                QDesktopServices::openUrl(QUrl("https://www.tarsnap.com/account.html"));
            });
    connect(_ui->statusBarLabel, &TextLabel::clicked,
            [=](){
                _ui->expandJournalButton->setChecked(!_ui->expandJournalButton->isChecked());
            });
}

MainWindow::~MainWindow()
{
    commitSettings();
    delete _ui;
}

void MainWindow::loadSettings()
{
    QSettings settings;
    _ui->accountUserLineEdit->setText(settings.value("tarsnap/user", "").toString());
    _ui->accountMachineKeyLineEdit->setText(settings.value("tarsnap/key", "").toString());
    _ui->accountMachineLineEdit->setText(settings.value("tarsnap/machine", "").toString());
    _ui->tarsnapPathLineEdit->setText(settings.value("tarsnap/path", "").toString());
    _ui->tarsnapCacheLineEdit->setText(settings.value("tarsnap/cache", "").toString());
    _ui->aggressiveNetworkingCheckBox->setChecked(settings.value("tarsnap/aggressive_networking", false).toBool());
    _ui->traverseMountCheckBox->setChecked(settings.value("tarsnap/traverse_mount", true).toBool());
    _ui->followSymLinksCheckBox->setChecked(settings.value("tarsnap/follow_symlinks", false).toBool());
    _ui->preservePathsCheckBox->setChecked(settings.value("tarsnap/preserve_pathnames", true).toBool());
    _useSIPrefixes = settings.value("app/si_prefixes", false).toBool();
    _ui->siPrefixesCheckBox->setChecked(_useSIPrefixes);
    _ui->skipFilesSpinBox->setValue(settings.value("app/skip_files_value", 0).toLongLong());
    _ui->downloadsDirLineEdit->setText(settings.value("app/downloads_dir", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString());
}

void MainWindow::initialize()
{
    emit loadArchives();
    emit loadJobs();
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    _tarsnapLogo.move(this->width()-_tarsnapLogo.width()-10,3);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        _windowDragPos = event->pos();
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        QPoint diff = event->pos() - _windowDragPos;
        QPoint newpos = this->pos() + diff;
        this->move(newpos);
    }
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if(isMaximized())
        this->showNormal();
    else
        this->showMaximized();
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    switch(event->key())
    {
    case Qt::Key_Escape:
        if((_ui->mainTabWidget->currentWidget() == _ui->archivesTab)
           &&(_ui->archiveDetailsWidget->isVisible()))
            _ui->archiveDetailsWidget->hide();
        if((_ui->mainTabWidget->currentWidget() == _ui->jobsTab)
           &&(_ui->jobDetailsWidget->isVisible()))
            hideJobDetails();
        else if(_ui->journalLog->isVisible())
            _ui->expandJournalButton->toggle();
        break;
    default:
        QWidget::keyReleaseEvent(event);
    }
}

void MainWindow::backupTaskUpdate(const TaskStatus& status)
{
    BackupTaskPtr backupTask = qobject_cast<BackupTaskPtr>(sender());
    switch (status) {
    case TaskStatus::Completed:
        updateStatusMessage(tr("Backup <i>%1</i> completed. (%2 new data on Tarsnap)")
                            .arg(backupTask->name()).arg(Utils::humanBytes(backupTask->archive()->sizeUniqueCompressed(), _useSIPrefixes))
                            , backupTask->archive()->archiveStats());
        delete backupTask;
        break;
    case TaskStatus::Queued:
        updateStatusMessage(tr("Backup <i>%1</i> queued.").arg(backupTask->name()));
        break;
    case TaskStatus::Running:
        updateStatusMessage(tr("Backup <i>%1</i> is running.").arg(backupTask->name()));
        break;
    case TaskStatus::Failed:
        updateStatusMessage(tr("Backup <i>%1</i> failed: %2").arg(backupTask->name()).arg(backupTask->output().simplified())
                           ,tr("%1").arg(backupTask->output()));
        delete backupTask;
        break;
    case TaskStatus::Paused:
        updateStatusMessage(tr("Backup <i>%1</i> paused.").arg(backupTask->name()));
        break;
    default:
        break;
    }
}

void MainWindow::archivesDeleted(QList<ArchivePtr> archives, bool done)
{
    if(archives.count() > 1)
    {
        QString detail(archives[0]->name());
        for(int i = 1; i < archives.count(); ++i)
        {
            ArchivePtr archive = archives.at(i);
            detail.append(QString::fromLatin1(", ") + archive->name());
        }
        updateStatusMessage(tr("Deleting archive <i>%1</i> and %2 more archives...%3")
                            .arg(archives.first()->name()).arg(archives.count()-1)
                            .arg(done?"done":""), detail);
    }
    else if(archives.count() == 1)
    {
        updateStatusMessage(tr("Deleting archive <i>%1</i>...%2").arg(archives.first()->name())
                            .arg(done?"done":""));
    }
}

void MainWindow::updateLoadingAnimation(bool idle)
{
    if(idle)
        _ui->busyWidget->stop();
    else
        _ui->busyWidget->animate();
}

void MainWindow::updateSettingsSummary(quint64 sizeTotal, quint64 sizeCompressed, quint64 sizeUniqueTotal, quint64 sizeUniqueCompressed, quint64 archiveCount, qreal credit, QString accountStatus)
{
    QString tooltip(tr("\t\tTotal size\tCompressed size\n"
                       "all archives\t%1\t\t%2\n"
                       "unique data\t%3\t\t%4").arg(sizeTotal).arg(sizeCompressed)
                       .arg(sizeUniqueTotal).arg(sizeUniqueCompressed));
    _ui->accountTotalSizeLabel->setText(Utils::humanBytes(sizeTotal, _useSIPrefixes));
    _ui->accountTotalSizeLabel->setToolTip(tooltip);
    _ui->accountActualSizeLabel->setText(Utils::humanBytes(sizeUniqueCompressed, _useSIPrefixes));
    _ui->accountActualSizeLabel->setToolTip(tooltip);
    _ui->accountStorageSavedLabel->setText(Utils::humanBytes(sizeTotal-sizeUniqueCompressed, _useSIPrefixes));
    _ui->accountStorageSavedLabel->setToolTip(tooltip);
    _ui->accountArchivesCountLabel->setText(QString::number(archiveCount));
    _ui->accountCreditLabel->setText(QString::number(credit, 'f'));
    _ui->accountStatusLabel->setText(accountStatus);
}

void MainWindow::repairCacheStatus(TaskStatus status, QString reason)
{
    switch (status) {
    case TaskStatus::Completed:
        updateStatusMessage(tr("Cache repair succeeded."), reason);
        break;
    case TaskStatus::Failed:
    default:
        updateStatusMessage(tr("Cache repair failed. Hover mouse for details."), reason);
        break;
    }
}

void MainWindow::purgeArchivesStatus(TaskStatus status, QString reason)
{
    switch (status) {
    case TaskStatus::Completed:
        updateStatusMessage(tr("All archives purged successfully."), reason);
        break;
    case TaskStatus::Failed:
    default:
        updateStatusMessage(tr("Archives purging failed. Hover mouse for details."), reason);
        break;
    }
}

void MainWindow::restoreArchiveStatus(ArchivePtr archive, TaskStatus status, QString reason)
{
    switch (status) {
    case TaskStatus::Completed:
        updateStatusMessage(tr("Restoring archive <i>%1</i>...done").arg(archive->name()), reason);
        break;
    case TaskStatus::Failed:
    default:
        updateStatusMessage(tr("Restoring archive <i>%1</i>...failed. Hover mouse for details.").arg(archive->name()), reason);
        break;
    }
}

void MainWindow::updateBackupItemTotals(quint64 count, quint64 size)
{
    if(count != 0)
    {
        _ui->backupDetailLabel->setText(tr("%1 %2 (%3)").arg(count).arg(count == 1? "item":"items").arg(Utils::humanBytes(size, _useSIPrefixes)));
        if(!_ui->backupNameLineEdit->text().isEmpty())
            _ui->backupButton->setEnabled(true);
    }
    else
    {
        _ui->backupDetailLabel->clear();
        _ui->backupButton->setEnabled(false);
    }
}

void MainWindow::displayInspectArchive(ArchivePtr archive)
{
    if(_currentArchiveDetail)
        disconnect(_currentArchiveDetail.data(), SIGNAL(changed()), this, SLOT(updateInspectArchive()));

    _currentArchiveDetail = archive;

    if(_currentArchiveDetail)
        connect(_currentArchiveDetail.data(), SIGNAL(changed()), this, SLOT(updateInspectArchive()));

    if(archive->sizeTotal() == 0)
        emit loadArchiveStats(archive);

    if(archive->contents().count() == 0)
        emit loadArchiveContents(archive);

    updateInspectArchive();

    if(!_ui->archiveDetailsWidget->isVisible())
        _ui->archiveDetailsWidget->show();

    if(_ui->mainTabWidget->currentWidget() != _ui->archivesTab)
        _ui->mainTabWidget->setCurrentWidget(_ui->archivesTab);

    _ui->archiveListWidget->setSelectedArchive(archive);
}

void MainWindow::appendTimestampCheckBoxToggled(bool checked)
{
    if(checked)
    {
        QString text = _ui->backupNameLineEdit->text();
        _lastTimestamp.clear();
        _lastTimestamp.append("_");
        _lastTimestamp.append(QDateTime::currentDateTime().toString("yyyy-MM-dd-HH:mm:ss"));
        text.append(_lastTimestamp);
        _ui->backupNameLineEdit->setText(text);
        _ui->backupNameLineEdit->setCursorPosition(0);
    }
    else
    {
        QString text = _ui->backupNameLineEdit->text();
        if(!_lastTimestamp.isEmpty() && !text.isEmpty())
        {
            int index = text.indexOf(_lastTimestamp, -(_lastTimestamp.count()));
            if(index != -1)
            {
                text.truncate(index);
                _ui->backupNameLineEdit->setText(text);
            }
        }
    }
}

void MainWindow::backupButtonClicked()
{
    QList<QUrl> urls;
    for(int i = 0; i < _ui->backupListWidget->count(); ++i)
    {
        urls << static_cast<BackupListItem*>(_ui->backupListWidget->item(i))->url();
    }
    BackupTaskPtr backup(new BackupTask);
    backup->setName(_ui->backupNameLineEdit->text());
    backup->setUrls(urls);
    connect(backup, SIGNAL(statusUpdate(const TaskStatus&)), this, SLOT(backupTaskUpdate(const TaskStatus&)), Qt::QueuedConnection);
    emit backupNow(backup);
    _ui->appendTimestampCheckBox->setChecked(false);
}

void MainWindow::updateInspectArchive()
{
    if(_currentArchiveDetail)
    {
        _ui->archiveNameLabel->setText(_currentArchiveDetail->name());
        _ui->archiveDateLabel->setText(_currentArchiveDetail->timestamp().toString());
        if(_currentArchiveDetail->jobRef().isEmpty())
        {
            _ui->archiveJobLabel->hide();
            _ui->archiveJobLabelField->hide();
        }
        else
        {
            _ui->archiveJobLabel->show();
            _ui->archiveJobLabelField->show();
            _ui->archiveJobLabel->setText(_currentArchiveDetail->jobRef());
        }
        _ui->archiveSizeLabel->setText(Utils::humanBytes(_currentArchiveDetail->sizeTotal(), _useSIPrefixes));
        _ui->archiveSizeLabel->setToolTip(_currentArchiveDetail->archiveStats());
        _ui->archiveUniqueDataLabel->setText(Utils::humanBytes(_currentArchiveDetail->sizeUniqueCompressed(), _useSIPrefixes));
        _ui->archiveUniqueDataLabel->setToolTip(_currentArchiveDetail->archiveStats());
        _ui->archiveCommandLabel->setText(_currentArchiveDetail->command());
        int count = _currentArchiveDetail->contents().count();
        _ui->archiveContentsLabel->setText(tr("Contents (%1)").arg((count == 0) ? tr("loading..."):QString::number(count)));
        _ui->archiveContentsPlainTextEdit->setPlainText(_currentArchiveDetail->contents().join('\n'));
    }
}

void MainWindow::updateStatusMessage(QString message, QString detail)
{
    _ui->statusBarLabel->setText(message);
    _ui->statusBarLabel->setToolTip(detail);

    appendToJournalLog(QString("[%1] %2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).arg(message));
}

void MainWindow::commitSettings()
{
    DEBUG << "COMMIT SETTINGS";
    QSettings settings;
    settings.setValue("tarsnap/path",    _ui->tarsnapPathLineEdit->text());
    settings.setValue("tarsnap/cache",   _ui->tarsnapCacheLineEdit->text());
    settings.setValue("tarsnap/key",     _ui->accountMachineKeyLineEdit->text());
    settings.setValue("tarsnap/machine", _ui->accountMachineLineEdit->text());
    settings.setValue("tarsnap/user",    _ui->accountUserLineEdit->text());
    settings.setValue("tarsnap/aggressive_networking", _ui->aggressiveNetworkingCheckBox->isChecked());
    settings.setValue("tarsnap/preserve_pathnames", _ui->preservePathsCheckBox->isChecked());
    settings.setValue("tarsnap/traverse_mount", _ui->traverseMountCheckBox->isChecked());
    settings.setValue("tarsnap/follow_symlinks", _ui->followSymLinksCheckBox->isChecked());
    settings.setValue("app/si_prefixes", _ui->siPrefixesCheckBox->isChecked());
    settings.setValue("app/skip_files_value", _ui->skipFilesSpinBox->value());
    settings.setValue("app/downloads_dir", _ui->downloadsDirLineEdit->text());
    settings.sync();
    emit settingsChanged();
}

void MainWindow::validateMachineKeyPath()
{
    QFileInfo machineKeyFile(_ui->accountMachineKeyLineEdit->text());
    if(machineKeyFile.exists() && machineKeyFile.isFile() && machineKeyFile.isReadable())
        _ui->accountMachineKeyLineEdit->setStyleSheet("QLineEdit {color: black;}");
    else
        _ui->accountMachineKeyLineEdit->setStyleSheet("QLineEdit {color: red;}");
}

void MainWindow::validateTarsnapPath()
{
    if(Utils::findTarsnapClientInPath(_ui->tarsnapPathLineEdit->text()).isEmpty())
        _ui->tarsnapPathLineEdit->setStyleSheet("QLineEdit {color: red;}");
    else
        _ui->tarsnapPathLineEdit->setStyleSheet("QLineEdit {color: black;}");
}

void MainWindow::validateTarsnapCache()
{
    if(Utils::validateTarsnapCache(_ui->tarsnapCacheLineEdit->text()).isEmpty())
        _ui->tarsnapCacheLineEdit->setStyleSheet("QLineEdit {color: red;}");
    else
        _ui->tarsnapCacheLineEdit->setStyleSheet("QLineEdit {color: black;}");
}

void MainWindow::purgeTimerFired()
{
    if(_purgeTimerCount <= 1)
    {
        _purgeTimer.stop();
        _purgeCountdownWindow.accept();
        emit purgeArchives();
        updateStatusMessage(tr("Archives purge initiated..."));
    }
    else
    {
        --_purgeTimerCount;
        _purgeCountdownWindow.setText(tr("Purging all archives in %1 seconds...").arg(_purgeTimerCount));
    }
}

void MainWindow::appendToJournalLog(QString msg)
{
    QTextCursor cursor(_ui->journalLog->document());
    if(!_ui->journalLog->document()->isEmpty())
    {
        cursor.movePosition(QTextCursor::End);
        cursor.insertBlock();
        cursor.movePosition(QTextCursor::NextBlock);
    }
    QColor bgcolor;
    int blockCount = _ui->journalLog->document()->blockCount();
    if (blockCount%2)
        bgcolor = qApp->palette().base().color();
    else
        bgcolor = qApp->palette().alternateBase().color();
    QTextBlockFormat bf;
    bf.setBackground(QBrush(bgcolor));
    cursor.mergeBlockFormat(bf);
    cursor.insertText(msg.remove(QRegExp("<[^>]*>"))); // also removes html tags
    _ui->journalLog->moveCursor(QTextCursor::End);
    _ui->journalLog->ensureCursorVisible();
}

void MainWindow::appendToConsoleLog(QString msg)
{
    _ui->consoleLogTextEdit->append(msg);
}

void MainWindow::browseForBackupItems()
{
    FilePickerDialog picker;
    if(picker.exec())
        QMetaObject::invokeMethod(_ui->backupListWidget, "addItemsWithUrls"
                                  , Qt::QueuedConnection, Q_ARG(QList<QUrl>
                                  , picker.getSelectedUrls()));
}

void MainWindow::accountMachineUseHostnameButtonClicked()
{
    _ui->accountMachineLineEdit->setText(QHostInfo::localHostName());
    commitSettings();
}

void MainWindow::accountMachineKeyBrowseButtonClicked()
{
    QString key = QFileDialog::getOpenFileName(this, tr("Browse for existing machine key"));
    if(!key.isEmpty())
    {
        _ui->accountMachineKeyLineEdit->setText(key);
        commitSettings();
    }
}

void MainWindow::tarsnapPathBrowseButtonClicked()
{
    QString tarsnapPath = QFileDialog::getExistingDirectory(this,
                                                            tr("Find Tarsnap client"),
                                                            _ui->tarsnapPathLineEdit->text());
    if(!tarsnapPath.isEmpty())
    {
        _ui->tarsnapPathLineEdit->setText(tarsnapPath);
        commitSettings();
    }
}

void MainWindow::tarsnapCacheBrowseButton()
{
    QString tarsnapCacheDir = QFileDialog::getExistingDirectory(this,
                                                            tr("Tarsnap cache location"),
                                                            _ui->tarsnapCacheLineEdit->text());
    if(!tarsnapCacheDir.isEmpty())
    {
        _ui->tarsnapCacheLineEdit->setText(tarsnapCacheDir);
        commitSettings();
    }
}

void MainWindow::repairCacheButtonClicked()
{
    emit repairCache();
}

void MainWindow::purgeArchivesButtonClicked()
{
    const QString confirmationText = tr("No Tomorrow");
    bool ok = false;
    QString userText = QInputDialog::getText(this,
        tr("Purge all archives?"),
        tr("This action will <b>delete all (%1) archives</b> stored for this key.<br /><br />To confirm, type '%2' and press OK.<br /><br /><i>Warning: This action cannot be undone. All archives will be <b>lost forever</b></i>.").arg(_ui->accountArchivesCountLabel->text(), confirmationText),
        QLineEdit::Normal,
        "",
        &ok);
    if (ok && confirmationText == userText)
    {
        _purgeTimerCount = PURGE_SECONDS_DELAY;

        _purgeCountdownWindow.setText(tr(
            "Purging all archives in %1 seconds...").arg(_purgeTimerCount));
        _purgeTimer.start(1000);

        if(QMessageBox::Cancel == _purgeCountdownWindow.exec())
        {
            _purgeTimer.stop();
            updateStatusMessage(tr("Purge cancelled."));
        }
    }
}

void MainWindow::runSetupWizardClicked()
{
    QMessageBox::StandardButton confirm = QMessageBox::question(this, tr("Confirm action")
                                                                ,tr("Reset current app settings, job definitions and run the setup wizard?")
                                                                ,( QMessageBox::Yes | QMessageBox::No ), QMessageBox::No);
    if(confirm == QMessageBox::Yes)
    {
        emit runSetupWizard();
    }
}

void MainWindow::expandJournalButtonToggled(bool checked)
{
    if(checked)
        _ui->journalLog->show();
    else
        _ui->journalLog->hide();
}

void MainWindow::downloadsDirBrowseButtonClicked()
{
    QString downDir = QFileDialog::getExistingDirectory(this,
                                                     tr("Browse for downloads directory"),
                                                     QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());
    if(!downDir.isEmpty())
    {
        _ui->downloadsDirLineEdit->setText(downDir);
        commitSettings();
    }
}

void MainWindow::displayJobDetails(JobPtr job)
{
    _ui->jobListWidget->scrollToItem(_ui->jobListWidget->currentItem(), QAbstractItemView::EnsureVisible);
    hideJobDetails();
    _ui->jobDetailsWidget->setJob(job);
    _ui->jobDetailsWidget->show();
    if(_ui->mainTabWidget->currentWidget() != _ui->jobsTab)
        _ui->mainTabWidget->setCurrentWidget(_ui->jobsTab);
}

void MainWindow::hideJobDetails()
{
    _ui->jobDetailsWidget->hide();
    if(_ui->addJobButton->property("save").toBool())
    {
        _ui->addJobButton->setText(tr("Add job"));
        _ui->addJobButton->setProperty("save", false);
        _ui->addJobButton->setEnabled(true);
    }
}

void MainWindow::addJobClicked()
{
    if(!_ui->addJobButton->isEnabled())
        return;

    if(_ui->addJobButton->property("save").toBool())
    {
        _ui->jobDetailsWidget->saveNew();
        _ui->addJobButton->setText(tr("Add job"));
        _ui->addJobButton->setProperty("save", false);
        _ui->addJobButton->setEnabled(true);
    }
    else
    {
        JobPtr job(new Job());
        displayJobDetails(job);
        _ui->addJobButton->setEnabled(false);
        _ui->addJobButton->setText(tr("Save"));
        _ui->addJobButton->setProperty("save", true);
    }
}

void MainWindow::cancelRunningTasks()
{
    QMessageBox::StandardButton confirm =
                                        QMessageBox::question(this,
                                        tr("Cancel running tasks"),
                                        tr("Stop the currently running tasks?"));
    if(confirm == QMessageBox::Yes)
    {
        updateStatusMessage("Stopping all running tasks.");
        emit stopTasks();
    }
}


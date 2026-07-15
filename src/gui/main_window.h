#pragma once

#include <QMainWindow>

class QTabWidget;
class QStatusBar;

namespace backer::gui {

class BackupTab;
class RestoreTab;
class ScheduleTab;

/// Main application window with tabbed interface for backup, restore,
/// and scheduled tasks.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    /// Called when a backup or restore operation completes.
    void onBackupFinished(bool success, QString message);

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();

    QTabWidget* tabWidget_{nullptr};
    BackupTab* backupTab_{nullptr};
    RestoreTab* restoreTab_{nullptr};
    ScheduleTab* scheduleTab_{nullptr};
};

} // namespace backer::gui

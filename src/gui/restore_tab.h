#pragma once

#include "cli/commands.h"

#include <QWidget>

#include <filesystem>

class QLineEdit;
class QPushButton;

namespace backer::gui {

class BackupWorker;
class LogWidget;
class ProgressWidget;

/// Restore configuration tab — select backup source and target directory.
/// All format/algorithm detection is automatic from filename.
class RestoreTab : public QWidget {
    Q_OBJECT
public:
    explicit RestoreTab(QWidget* parent = nullptr);

signals:
    void restoreFinished(bool success, QString message);

private slots:
    void onBrowseSource();
    void onBrowseDest();
    void onStartRestore();
    void onCancel();
    void onRestoreFinished(bool success, QString const& msg);

private:
    void setupUi();
    void setupConnections();

    QLineEdit* sourcePath_{nullptr};
    QLineEdit* destPath_{nullptr};
    QLineEdit* password_{nullptr};
    QPushButton* startBtn_{nullptr};
    QPushButton* cancelBtn_{nullptr};
    ProgressWidget* progressWidget_{nullptr};
    LogWidget* logWidget_{nullptr};
    BackupWorker* worker_{nullptr};
};

} // namespace backer::gui

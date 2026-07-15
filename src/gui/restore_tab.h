#pragma once

#include "cli/commands.h"

#include <QWidget>

#include <filesystem>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

namespace backer::gui {

class BackupWorker;
class LogWidget;
class ProgressWidget;

/// Restore configuration tab — select backup source, target directory,
/// decompression/pack options, and run restore.
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

private:
    void setupUi();
    void setupConnections();

    QLineEdit* sourcePath_{nullptr};
    QLineEdit* destPath_{nullptr};
    QCheckBox* enableDecompress_{nullptr};
    QComboBox* decompressAlgo_{nullptr};
    QCheckBox* enableDecrypt_{nullptr};
    QComboBox* decryptAlgo_{nullptr};
    QLineEdit* password_{nullptr};
    QCheckBox* enablePack_{nullptr};
    QComboBox* packFormat_{nullptr};
    QCheckBox* preserveMetadata_{nullptr};
    QCheckBox* handleSpecial_{nullptr};
    QPushButton* startBtn_{nullptr};
    QPushButton* cancelBtn_{nullptr};
    ProgressWidget* progressWidget_{nullptr};
    LogWidget* logWidget_{nullptr};
    BackupWorker* worker_{nullptr};
};

} // namespace backer::gui

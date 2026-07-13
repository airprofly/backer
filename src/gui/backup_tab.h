#pragma once

#include "cli/commands.h"

#include <QDateTime>
#include <QString>
#include <QWidget>

#include <filesystem>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace backer::gui {

class BackupWorker;
class FilterDialog;
class LogWidget;
class ProgressWidget;

/// Backup configuration tab — source/dest, pack/compress/encrypt options,
/// filter criteria, progress display, and log output.
class BackupTab : public QWidget {
    Q_OBJECT
public:
    explicit BackupTab(QWidget* parent = nullptr);

signals:
    /// Emitted when a backup operation completes.
    void backupFinished(bool success, QString message);

private slots:
    void onBrowseSource();
    void onBrowseDest();
    void onStartBackup();
    void onCancel();
    void onEditFilter();
    void onPackToggled(bool checked);
    void onCompressToggled(bool checked);
    void onEncryptToggled(bool checked);
    void onResetDefaults();
    void onBackupFinished(bool success, QString const& message);

private:
    void setupUi();
    void setupConnections();
    backer::cli::BackupOptions collectOptions() const;

    // ── Source / destination ──────────────────────────────────
    QLineEdit* sourcePath_{nullptr};
    QLineEdit* destPath_{nullptr};
    QPushButton* browseSourceBtn_{nullptr};
    QPushButton* browseDestBtn_{nullptr};

    // ── Filter ────────────────────────────────────────────────
    QCheckBox* enableFilter_{nullptr};
    QPushButton* editFilterBtn_{nullptr};
    QGroupBox* filterGroup_{nullptr};
    QLineEdit* includePaths_{nullptr};
    QLineEdit* excludePaths_{nullptr};
    QLineEdit* includeTypes_{nullptr};
    QLineEdit* excludeTypes_{nullptr};

    // ── Pack / compress / encrypt ─────────────────────────────
    QCheckBox* enablePack_{nullptr};
    QComboBox* packFormat_{nullptr};
    QCheckBox* enableCompress_{nullptr};
    QComboBox* compressAlgo_{nullptr};
    QSpinBox* compressLevel_{nullptr};
    QCheckBox* enableEncrypt_{nullptr};
    QComboBox* encryptAlgo_{nullptr};
    QLineEdit* password_{nullptr};
    QLineEdit* confirmPassword_{nullptr};

    // ── Action buttons ────────────────────────────────────────
    QPushButton* startBtn_{nullptr};
    QPushButton* cancelBtn_{nullptr};

    // ── Filter state from FilterDialog (time / size / owner) ──
    bool hasTimeFilter_{false};
    QDateTime mtimeAfter_;
    QDateTime mtimeBefore_;
    bool hasSizeFilter_{false};
    qint64 sizeMin_{0};
    qint64 sizeMax_{0};
    QString owner_;

    // ── Progress & log ────────────────────────────────────────
    ProgressWidget* progressWidget_{nullptr};
    LogWidget* logWidget_{nullptr};

    // ── Worker ────────────────────────────────────────────────
    BackupWorker* worker_{nullptr};
};

} // namespace backer::gui
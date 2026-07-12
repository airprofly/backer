#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QDialogButtonBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace backer::gui {

/// Visual filter criteria editor dialog.
/// Allows configuring path includes/excludes, file type filters,
/// time range, size range, and owner filter.
class FilterDialog : public QDialog {
    Q_OBJECT
public:
    explicit FilterDialog(QWidget* parent = nullptr);

    // ── Setters (pre-fill from existing config) ─────────────
    void setIncludePaths(QStringList const& paths);
    void setExcludePaths(QStringList const& paths);

    // ── Getters ─────────────────────────────────────────────
    QStringList includePaths() const;
    QStringList excludePaths() const;
    QStringList includeTypes() const;
    QStringList excludeTypes() const;

    bool hasTimeFilter() const;
    QDateTime mtimeAfter() const;
    QDateTime mtimeBefore() const;

    bool hasSizeFilter() const;
    qint64 sizeMin() const;
    qint64 sizeMax() const;

    QString owner() const;

private slots:
    void onAddIncludePath();
    void onRemoveIncludePath();
    void onAddExcludePath();
    void onRemoveExcludePath();

private:
    void setupUi();

    // ── Path filters ────────────────────────────────────────
    QListWidget* includePathList_{nullptr};
    QListWidget* excludePathList_{nullptr};

    // ── Type checkboxes ─────────────────────────────────────
    QCheckBox* typeFile_{nullptr};
    QCheckBox* typeDir_{nullptr};
    QCheckBox* typeSymlink_{nullptr};
    QCheckBox* typeFifo_{nullptr};
    QCheckBox* typeBlock_{nullptr};
    QCheckBox* typeChar_{nullptr};
    QCheckBox* typeSocket_{nullptr};

    // ── Time / size ─────────────────────────────────────────
    QCheckBox* enableTimeFilter_{nullptr};
    QDateTimeEdit* mtimeAfter_{nullptr};
    QDateTimeEdit* mtimeBefore_{nullptr};
    QCheckBox* enableSizeFilter_{nullptr};
    QSpinBox* sizeMin_{nullptr};
    QSpinBox* sizeMax_{nullptr};
    QComboBox* sizeUnit_{nullptr};

    // ── Owner ───────────────────────────────────────────────
    QLineEdit* owner_{nullptr};

    QDialogButtonBox* buttonBox_{nullptr};
};

} // namespace backer::gui

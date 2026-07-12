#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace backer::gui {

/// Settings tab — default paths, compression preferences, log level, etc.
/// Persists settings via QSettings.
class SettingsTab : public QWidget {
    Q_OBJECT
public:
    explicit SettingsTab(QWidget* parent = nullptr);

private slots:
    void onBrowseDefaultSource();
    void onBrowseDefaultDest();
    void onSave();
    void onRestoreDefaults();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    QLineEdit* defaultSource_{nullptr};
    QLineEdit* defaultDest_{nullptr};
    QComboBox* defaultPack_{nullptr};
    QComboBox* defaultCompress_{nullptr};
    QSpinBox* defaultCompressLevel_{nullptr};
    QComboBox* defaultEncrypt_{nullptr};
    QComboBox* logLevel_{nullptr};
    QSpinBox* threadCount_{nullptr};
};

} // namespace backer::gui

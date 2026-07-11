#pragma once

#include <QWidget>

class QPushButton;
class QTextEdit;

namespace backer::gui {

/// Read-only log display with color-coded messages (info/warn/error)
/// and automatic timestamping.
class LogWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogWidget(QWidget* parent = nullptr);

public slots:
    /// Append a message at the given severity level.
    /// @param level 0 = info (black), 1 = warn (dark orange), 2 = error (crimson)
    void appendMessage(QString const& msg, int level);
    /// Clear all log content.
    void clear();

private:
    void setupUi();

    QTextEdit* logView_{nullptr};
    QPushButton* clearButton_{nullptr};
};

} // namespace backer::gui

#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;

namespace backer::gui {

/// Compact progress display widget showing a progress bar,
/// current file name, statistics, and transfer speed.
class ProgressWidget : public QWidget {
    Q_OBJECT
public:
    explicit ProgressWidget(QWidget* parent = nullptr);

public slots:
    void setRange(int min, int max);
    void setValue(int value);
    void setCurrentFile(QString const& file);
    void setStats(int filesDone, int filesTotal,
                  qint64 bytesDone, qint64 bytesTotal);
    void setSpeed(double mbPerSec);
    void reset();
    /// Show or hide the widget. When stopped (false), resets values.
    void setRunning(bool running);

private:
    void setupUi();

    QProgressBar* progressBar_{nullptr};
    QLabel* currentFileLabel_{nullptr};
    QLabel* statsLabel_{nullptr};
    QLabel* speedLabel_{nullptr};
};

} // namespace backer::gui

#include "gui/progress_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

namespace backer::gui {

ProgressWidget::ProgressWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setRunning(false);
}

void ProgressWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Current file label (elide middle)
    currentFileLabel_ = new QLabel(this);
    currentFileLabel_->setTextFormat(Qt::PlainText);
    currentFileLabel_->setText(QString());
    // Elide in the middle so the beginning and end of the path are visible
    QFontMetrics fm(currentFileLabel_->font());
    currentFileLabel_->setText(fm.elidedText(QString(), Qt::ElideMiddle, 500));

    // Stats + speed on the same line
    auto* statsRow = new QHBoxLayout();

    statsLabel_ = new QLabel(this);
    statsLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    speedLabel_ = new QLabel(this);
    speedLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    statsRow->addWidget(statsLabel_);
    statsRow->addStretch();
    statsRow->addWidget(speedLabel_);

    // Progress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);

    mainLayout->addWidget(currentFileLabel_);
    mainLayout->addLayout(statsRow);
    mainLayout->addWidget(progressBar_);
}

void ProgressWidget::setRange(int min, int max)
{
    progressBar_->setRange(min, max);
}

void ProgressWidget::setValue(int value)
{
    progressBar_->setValue(value);

    // Green when 100%
    if (value == progressBar_->maximum() && progressBar_->maximum() > 0) {
        progressBar_->setStyleSheet(
            QStringLiteral("QProgressBar::chunk { background-color: #4CAF50; }"));
    } else {
        progressBar_->setStyleSheet(QString());
    }
}

void ProgressWidget::setCurrentFile(QString const& file)
{
    QFontMetrics fm(currentFileLabel_->font());
    currentFileLabel_->setText(fm.elidedText(file, Qt::ElideMiddle, 500));
}

void ProgressWidget::setStats(int filesDone, int filesTotal,
                              qint64 bytesDone, qint64 bytesTotal)
{
    // Format bytes in human-readable form
    auto formatBytes = [](qint64 bytes) -> QString {
        if (bytes < 1024) {
            return QStringLiteral("%1 B").arg(bytes);
        }
        if (bytes < 1024 * 1024) {
            return QStringLiteral("%1 KB").arg(bytes / 1024);
        }
        double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
    };

    statsLabel_->setText(
        QStringLiteral("已备份: %1/%2 个文件, "
                       "%3 / %4") // "已备份: X/Y 个文件, A / B"
            .arg(filesDone)
            .arg(filesTotal)
            .arg(formatBytes(bytesDone))
            .arg(formatBytes(bytesTotal)));
}

void ProgressWidget::setSpeed(double mbPerSec)
{
    speedLabel_->setText(
        QStringLiteral("速度: %1 MB/s").arg(mbPerSec, 0, 'f', 1));
        // "速度: X.X MB/s"
}

void ProgressWidget::reset()
{
    progressBar_->setValue(0);
    progressBar_->setStyleSheet(QString());
    currentFileLabel_->setText(QString());
    statsLabel_->setText(QString());
    speedLabel_->setText(QString());
}

void ProgressWidget::setRunning(bool running)
{
    setVisible(running);
    if (!running) {
        reset();
    }
}

} // namespace backer::gui

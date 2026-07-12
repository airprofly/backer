#include "gui/log_widget.h"

#include <QDateTime>
#include <QFontDatabase>
#include <QGroupBox>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace backer::gui {

LogWidget::LogWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void LogWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Group box with title "日志"
    auto* groupBox = new QGroupBox(QStringLiteral("日志"), this);
    auto* groupLayout = new QVBoxLayout(groupBox);

    // Top row: clear button on the right
    auto* headerLayout = new QHBoxLayout();
    headerLayout->addStretch();

    clearButton_ = new QPushButton(QStringLiteral("清除"), groupBox);
    // "清除"
    clearButton_->setObjectName(QStringLiteral("logClearButton"));
    clearButton_->setFixedWidth(80);
    headerLayout->addWidget(clearButton_);

    groupLayout->addLayout(headerLayout);

    // Log text view (read-only, monospace)
    logView_ = new QTextEdit(groupBox);
    logView_->setReadOnly(true);
    logView_->setUndoRedoEnabled(false);
    logView_->setLineWrapMode(QTextEdit::NoWrap);
    logView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    // Monospace font
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (monoFont.family().isEmpty()) {
        monoFont = QFont(QStringLiteral("Courier New"), 10);
    }
    monoFont.setPointSize(10);
    logView_->setFont(monoFont);

    groupLayout->addWidget(logView_);
    mainLayout->addWidget(groupBox);

    // Connect clear button
    connect(clearButton_, &QPushButton::clicked,
            this, &LogWidget::clear);
}

void LogWidget::appendMessage(QString const& msg, int level)
{
    // Timestamp
    QString timestamp = QDateTime::currentDateTime().toString(
        QStringLiteral("[HH:mm:ss] "));

    // Color by level
    QString color;
    switch (level) {
        case 1:  color = QStringLiteral("#FF8C00"); break;  // dark orange
        case 2:  color = QStringLiteral("#DC143C"); break;  // crimson
        default: color = QStringLiteral("#000000"); break;  // black
    }

    QString html = QStringLiteral("<span style=\"color:%1\">%2%3</span><br>")
                       .arg(color, timestamp, msg.toHtmlEscaped());

    logView_->moveCursor(QTextCursor::End);
    logView_->insertHtml(html);

    // Trim to max 10000 lines
    QTextDocument* doc = logView_->document();
    if (doc->blockCount() > 10000) {
        QTextCursor cursor(doc->begin());
        cursor.select(QTextCursor::BlockUnderCursor);
        cursor.removeSelectedText();
        cursor.deleteChar(); // remove the trailing newline
    }

    // Auto-scroll to bottom
    QScrollBar* scrollBar = logView_->verticalScrollBar();
    if (scrollBar) {
        scrollBar->setValue(scrollBar->maximum());
    }
}

void LogWidget::clear()
{
    logView_->clear();
}

} // namespace backer::gui

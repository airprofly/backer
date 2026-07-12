#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace backer::gui {

/// Scheduled backup task management tab — view, add, edit, delete,
/// and manually trigger scheduled backup tasks.
class ScheduleTab : public QWidget {
    Q_OBJECT
public:
    explicit ScheduleTab(QWidget* parent = nullptr);

private slots:
    void onAddTask();
    void onEditTask();
    void onDeleteTask();
    void onRunNow();

private:
    void setupUi();
    void updateView();

    QStackedWidget* stack_{nullptr};
    QWidget* emptyPage_{nullptr};
    QTableWidget* taskTable_{nullptr};
    QPushButton* addBtn_{nullptr};
    QPushButton* editBtn_{nullptr};
    QPushButton* deleteBtn_{nullptr};
    QPushButton* runNowBtn_{nullptr};
};

} // namespace backer::gui

#pragma once

#include <QBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QToolButton>
#include <QWidget>
#include <QTimer>

#include "../err.hpp"
#include "../io/decl.hxx"
#include "../MutexGuard.hpp"
#include "../io/Task.hpp"

namespace cornus::gui {

class TaskGui: public QWidget {
public:
	~TaskGui();
	static TaskGui* From(io::Task *task, gui::TasksWin *tw);
	
	virtual QSize sizeHint() const override;
	virtual QSize minimumSizeHint() const override;
	
	void CheckTaskState();
	void TaskStateChanged(const io::TaskState new_state);
	
private:
	TaskGui(io::Task *task);
	void CreateGui();
	void ProcessAction(const QString &action);
	void UpdateSpeedLabel();
	
	NO_ASSIGN_COPY_MOVE(TaskGui);
	
	io::Task *task_ = nullptr;
	QBoxLayout *layout_ = nullptr;
	QProgressBar *progress_bar_ = nullptr;
	TasksWin *tasks_win_ = nullptr;
	bool made_visible_once_ = false;
	QLabel *info_ = nullptr;
	QLabel *speed_ = nullptr;
	QTimer *timer_ = nullptr;
	QToolButton *play_pause_btn_ = nullptr;
	QToolButton *cancel_button_ = nullptr;
	cornus::io::Progress progress_ = {};
	QIcon continue_icon_, pause_icon_;
};
}

#pragma once

#include <QBoxLayout>
#include <QProgressBar>
#include <QWidget>

#include "../err.hpp"
#include "../io/decl.hxx"
#include "../MutexGuard.hpp"

namespace cornus::gui {

class TaskGui: public QWidget {
public:
	~TaskGui();
	static TaskGui* From(io::Task *task, gui::TasksWin *tw);
	
	virtual QSize sizeHint() const override;
	virtual QSize minimumSizeHint() const override;
	io::Task* task() const { return task_; }
	
	void CheckTaskState();
	
private:
	TaskGui(io::Task *task);
	void CreateGui();
	void ProcessAction(const QString &action);
	
	NO_ASSIGN_COPY_MOVE(TaskGui);
	
	io::Task *task_ = nullptr;
	QBoxLayout *layout_ = nullptr;
	QProgressBar *progress_ = nullptr;
	TasksWin *tasks_win_ = nullptr;
};
}

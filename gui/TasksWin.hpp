#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../err.hpp"
#include "../io/Task.hpp"

#include <QBoxLayout>
#include <QMainWindow>

namespace cornus::gui {

class TasksWin: public QMainWindow {
	Q_OBJECT
public:
	TasksWin();
	~TasksWin();
	
	virtual QSize sizeHint() const override;
	virtual QSize minimumSizeHint() const override;
	QSize maximumSize() const;
	
	void TaskDone(TaskGui *gui_task, const io::TaskState state);
	
public Q_SLOTS:
	void add(cornus::io::Task *task);
	
private:
	NO_ASSIGN_COPY_MOVE(TasksWin);
	
	void CreateGui();
	
	QBoxLayout *layout_ = nullptr;
	QWidget *main_widget_ = nullptr;
	QSize screen_sz_ = {};
	int win_w_ = -1;
};

}

#include "TasksWin.hpp"

#include "../MutexGuard.hpp"
#include "TaskGui.hpp"

namespace cornus::gui {

TasksWin::TasksWin()
{
	CreateGui();
	setWindowIcon(QIcon(cornus::AppIconPath));
	setWindowTitle("I/O operations");
}

TasksWin::~TasksWin() {
}

void
TasksWin::add(io::Task *task)
{
	auto *task_gui = TaskGui::From(task, this);
	
	if (task_gui != nullptr)
		layout_->addWidget(task_gui);
}

void
TasksWin::CreateGui()
{
	main_widget_ = new QWidget();
	setCentralWidget(main_widget_);
	layout_ = new QBoxLayout(QBoxLayout::TopToBottom);
	main_widget_->setLayout(layout_);
}

void TasksWin::TaskDone(TaskGui *tg) {
	layout_->removeWidget(tg);
	delete tg;
	
	if (layout_->count() == 0) {
		mtl_info("no more tasks! hiding");
		setVisible(false);
	}
}

}

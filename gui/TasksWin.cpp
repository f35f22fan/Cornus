#include "TasksWin.hpp"

#include "../MutexGuard.hpp"
#include "TaskGui.hpp"

namespace cornus::gui {

TasksWin::TasksWin()
{
	CreateGui();
	setWindowIcon(QIcon(cornus::AppIconPath));
	setWindowTitle("I/O operations");
	show();
	raise();
}

TasksWin::~TasksWin() {
}

void
TasksWin::add(io::Task *task)
{
	auto *gui = TaskGui::From(task, this);
	if (gui == nullptr)
		return;
	
	data_.vec.append(gui);
	layout_->addWidget(gui);
	adjustSize();
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
		mtl_info("no more tasks! hiding/exiting");
		delete this;
	}
}

}

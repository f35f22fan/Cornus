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
	
	if (task_gui != nullptr) {
		layout_->addWidget(task_gui);
		adjustSize();
	}
}

void
TasksWin::CreateGui()
{
	main_widget_ = new QWidget();
	setCentralWidget(main_widget_);
	layout_ = new QBoxLayout(QBoxLayout::TopToBottom);
	main_widget_->setLayout(layout_);
}

QSize TasksWin::minimumSizeHint() const { return sizeHint(); }

QSize TasksWin::sizeHint() const {
	const int count = layout_->count();
	if (count == 0)
		return QSize(0, 0);
	auto *item = layout_->itemAt(0);
	QSize sz = item->sizeHint();
	return QSize(sz.width(), sz.height() * count);
}

QSize TasksWin::maximumSize() const { return sizeHint(); }


void TasksWin::TaskDone(TaskGui *tg, const io::TaskState state)
{
	layout_->removeWidget(tg);
	delete tg;
	adjustSize();
	
	if (layout_->count() == 0) {
///		mtl_info("no more tasks! hiding");
		setVisible(false);
	}
}

}

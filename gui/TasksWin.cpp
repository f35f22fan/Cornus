#include "TasksWin.hpp"

#include "../MutexGuard.hpp"
#include "TaskGui.hpp"

#include <QGuiApplication>
#include <QScreen>

namespace cornus::gui {

TasksWin::TasksWin()
{
	setFocusPolicy(Qt::WheelFocus);
	CreateGui();
	setWindowIcon(QIcon(cornus::AppIconPath));
	setWindowTitle("Cornus I/O Daemon");
	QList<QScreen*> screens = QGuiApplication::screens();
	if (!screens.isEmpty()) {
		screen_sz_ = screens[0]->availableSize();
	} else {
		screen_sz_ = QSize(1920, 1080);
	}
	
	win_w_ = std::max(800, screen_sz_.width() / 3);
	resize(win_w_, 100);
}

TasksWin::~TasksWin() {
}

void TasksWin::add(cornus::io::Task *task)
{
	auto *task_gui = TaskGui::From(task, this);
	
	if (task_gui != nullptr) {
		layout_->addWidget(task_gui);
		adjustSize();
	}
}

void TasksWin::CreateGui()
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
	return QSize(win_w_, sz.height() * count);
}

QSize TasksWin::maximumSize() const {
	return QSize(win_w_, sizeHint().height());
}

void TasksWin::TaskDone(TaskGui *gui_task, const io::TaskState state)
{
	layout_->removeWidget(gui_task);
	delete gui_task;
	adjustSize();
	
	if (layout_->count() == 0) {
		//mtl_info("No more tasks! hiding");
		setVisible(false);
	}
}

}

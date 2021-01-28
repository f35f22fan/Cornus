#include "TaskGui.hpp"

#include "actions.hxx"
#include "../io/Task.hpp"
#include "TasksWin.hpp"

#include <QTimer>
#include <QToolButton>

namespace cornus::gui {

const int ProgressMin = 0;
const int ProgressMax = 100;

TaskGui::TaskGui(io::Task *task): task_(task)
{
	CreateGui();
	mtl_info("changing state to continue");
	task_->data().ChangeState(io::TaskState::Continue);
	mtl_info("Done");
	
	QTimer *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, QOverload<>::of(&TaskGui::CheckTaskState));
	timer->start(1000);
}

TaskGui::~TaskGui()
{
	if (task_->data().WaitFor(io::TaskState::Finished)) {
		delete task_;
	} else {
		mtl_trace();
	}
}

void TaskGui::CheckTaskState()
{
	mtl_info("Checking io::Task state..");
	io::TaskState state = task_->data().GetState();
	
	if (state == io::TaskState::Finished) {
		mtl_info("Finished! deleting myself!");
		tasks_win_->TaskDone(this);
	}
	
}

void TaskGui::CreateGui()
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout_ = new QBoxLayout(QBoxLayout::LeftToRight);
	setLayout(layout_);
	
	progress_ = new QProgressBar();
	layout_->addWidget(progress_);
	progress_->setRange(ProgressMin, ProgressMax);
	progress_->setValue(75);
	
	auto *btn = new QToolButton();
	btn->setIcon(QIcon::fromTheme(QLatin1String("media-playback-start")));
	connect(btn, &QToolButton::clicked, [=] {
		ProcessAction(actions::IOContinue);});
	
	layout_->addWidget(btn);
}

TaskGui*
TaskGui::From(io::Task *task, TasksWin *tw)
{
	auto *g = new TaskGui(task);
	g->tasks_win_ = tw;
	
	return g;
}

void
TaskGui::ProcessAction(const QString &action) {
	mtl_printq(action);
}

QSize TaskGui::minimumSizeHint() const { return sizeHint(); }

QSize TaskGui::sizeHint() const {
	int lh = fontMetrics().boundingRect("Abg").height();
	return QSize(lh * 30, lh * 2);
}

}

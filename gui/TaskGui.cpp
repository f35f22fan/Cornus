#include "TaskGui.hpp"

#include "actions.hxx"
#include "../io/io.hh"
#include "../io/Task.hpp"
#include "TasksWin.hpp"

#include <QTimer>

namespace cornus::gui {

const QString IconCancel = QLatin1String("edit-delete");
const int ProgressMin = 0;
const int ProgressMax = 100;

TaskGui::TaskGui(io::Task *task): task_(task)
{
	continue_icon_ = QIcon::fromTheme(QLatin1String("media-playback-start"));
	pause_icon_ = QIcon::fromTheme(QLatin1String("media-playback-pause"));
	
	CreateGui();
	task_->data().ChangeState(io::TaskState::Continue);
	
	timer_ = new QTimer(this);
	connect(timer_, &QTimer::timeout, this,
		QOverload<>::of(&TaskGui::CheckTaskState));
	timer_->start(200);
}

TaskGui::~TaskGui()
{
	if (task_->data().WaitFor(io::TaskState::Finished | io::TaskState::Cancel)) {
		delete task_;
	} else {
		mtl_trace();
	}
}

void TaskGui::CheckTaskState()
{
	static io::TaskState last_state = io::TaskState::None;
	const io::TaskState state = task_->data().GetState();

	if (state & (io::TaskState::Finished | io::TaskState::Cancel)) {
		mtl_info("Finished! Removing task GUI");
		tasks_win_->TaskDone(this, state);
		return;
	}

	if (!made_visible_once_) {
		tasks_win_->adjustSize();
		tasks_win_->setVisible(true);
		made_visible_once_ = true;
	}
	
	if (state & io::TaskState::Error) {
		info_->setText(tr("An error occurred"));
		setVisible(true);
		timer_->stop();
		return;
	}
	
	if (state & io::TaskState::Continue) {
		auto prev_id = progress_.details_id;
		task_->progress().Get(progress_);
		UpdateSpeedLabel();
		int at = progress_.at / (progress_.total / 100);
		progress_bar_->setValue(at);
		if (prev_id != progress_.details_id) {
			info_->setText(progress_.details);
		}
		if (last_state != state) {
			play_pause_btn_->setIcon(pause_icon_);
			last_state = state;
		}
	} else if (state & io::TaskState::Pause) {
		timer_->stop();
		if (last_state != state) {
			play_pause_btn_->setIcon(continue_icon_);
			last_state = state;
		}
	}
}

void TaskGui::CreateGui()
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout_ = new QBoxLayout(QBoxLayout::LeftToRight);
	setLayout(layout_);
	
	{
		QBoxLayout *horiz = new QBoxLayout(QBoxLayout::TopToBottom);
		layout_->addLayout(horiz);
		
		progress_bar_ = new QProgressBar();
		horiz->addWidget(progress_bar_);
		progress_bar_->setRange(ProgressMin, ProgressMax);
		progress_bar_->setValue(75);
		progress_bar_->setTextVisible(true);
		
		QBoxLayout *two_label_layout = new QBoxLayout(QBoxLayout::LeftToRight);
		horiz->addLayout(two_label_layout);
		
		speed_ = new QLabel();
		two_label_layout->addWidget(speed_);
		info_ = new QLabel();
		two_label_layout->addWidget(info_);
	}
	
	{
		auto state = task_->data().GetState();
		QIcon &icon = (state == io::TaskState::Continue) ?
			pause_icon_ : continue_icon_;
		play_pause_btn_ = new QToolButton();
		play_pause_btn_->setIcon(icon);
		connect(play_pause_btn_, &QToolButton::clicked, [=] {
			ProcessAction(actions::IOContinue);
		});
		layout_->addWidget(play_pause_btn_);
	}
	{
		auto *btn = new QToolButton();
		btn->setIcon(QIcon::fromTheme(IconCancel));
		connect(btn, &QToolButton::clicked, [=] {
			ProcessAction(actions::IOCancel);
		});
		layout_->addWidget(btn);
	}
}

TaskGui*
TaskGui::From(io::Task *task, TasksWin *tw)
{
	auto *g = new TaskGui(task);
	g->tasks_win_ = tw;
	
	return g;
}

void
TaskGui::ProcessAction(const QString &action)
{
	auto &data = task_->data();
	if (action == actions::IOContinue) {
		auto state = data.GetState();
		
		if (state & io::TaskState::Continue) {
			data.ChangeState(io::TaskState::Pause);
			play_pause_btn_->setIcon(pause_icon_);
		} else if (state & io::TaskState::Pause) {
			data.ChangeState(io::TaskState::Continue);
			play_pause_btn_->setIcon(continue_icon_);
			timer_->start();
		}
	} else if (action == actions::IOCancel) {
		data.ChangeState(io::TaskState::Cancel);
		timer_->start();
	}
}

QSize TaskGui::minimumSizeHint() const { return sizeHint(); }

QSize TaskGui::sizeHint() const {
	int lh = fontMetrics().boundingRect("Abg").height();
	return QSize(lh * 30, lh * 2);
}

void TaskGui::UpdateSpeedLabel()
{
	if (progress_.time_worked < 1000) {
		/// if progress is less than 1 second then the speed
		/// counting algorithm simply doesn't work properly.
		return;
	}
	
	const double seconds = double(progress_.time_worked) / 1000;
	const i64 n = double(progress_.at) / seconds;
	QString s = io::SizeToString(n) + QLatin1String("/s");
	//mtl_info("seconds: %.2f, n: %ld, progress_.at: %ld", seconds, n, progress_.at);
	//mtl_printq(s);
	speed_->setText(s);
}

}

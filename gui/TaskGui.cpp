#include "TaskGui.hpp"

#include "actions.hxx"
#include "../io/io.hh"
#include "../io/Task.hpp"
#include "TasksWin.hpp"

#include <QBoxLayout>
#include <QPushButton>
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
	
	timer_ = new QTimer(this);
	connect(timer_, &QTimer::timeout, this,
		QOverload<>::of(&TaskGui::CheckTaskState));
	timer_->start(200);
	
	auto new_state = task_->data().GetState(nullptr);
	TaskStateChanged(new_state);
}

TaskGui::~TaskGui()
{
	if (task_->data().WaitFor(io::TaskState::Finished | io::TaskState::Abort)) {
		delete task_;
	} else {
		mtl_trace();
	}
}

void TaskGui::CheckTaskState()
{
	static io::TaskState last_state = io::TaskState::None;
	const io::TaskState state = task_->data().GetState(&task_question_);

	if (state & (io::TaskState::Finished | io::TaskState::Abort)) {
///		mtl_info("Finished! Removing task GUI");
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
		
		if (progress_.total != 0) { /// to avoid division by zero
			UpdateSpeedLabel();
			int at = progress_.at / (progress_.total / 100);
			progress_bar_->setValue(at);
		}
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
	} else if (state & io::TaskState::AwatingAnswer) {
		timer_->stop();
		
		switch (task_question_.question) {
		case io::Question::FileExists: {
			PresentUserFileExistsQuestion();
			break;
		}
		case io::Question::WriteFailed: {
			PresentUserWriteFailedQuestion();
			break;
		}
		default: {
			mtl_trace();
			break;
		}
		}
	} else {
		mtl_info("state: %u", u16(state));
	}
}

QWidget*
TaskGui::CreateFileExistsPane()
{
	QWidget *pane = new QWidget();
	auto *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	pane->setLayout(vert_layout);
	
	file_exists_list_.line_edit = new QLineEdit();
	file_exists_list_.line_edit->setReadOnly(true);
	vert_layout->addWidget(file_exists_list_.line_edit);
	
	QBoxLayout *hlayout = new QBoxLayout(QBoxLayout::LeftToRight);
	vert_layout->addLayout(hlayout);
	
	{
		auto *btn = new QPushButton(tr("Skip"));
		connect(btn, &QPushButton::clicked, [=] {
			SendFileExistsAnswer(io::FileExistsAnswer::Skip);
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Skip All"));
		connect(btn, &QPushButton::clicked, [=] {
			SendFileExistsAnswer(io::FileExistsAnswer::SkipAll);
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Auto Rename"));
		connect(btn, &QPushButton::clicked, [=] {
			SendFileExistsAnswer(io::FileExistsAnswer::AutoRename);
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Auto Rename All"));
		connect(btn, &QPushButton::clicked, [=] {
			SendFileExistsAnswer(io::FileExistsAnswer::AutoRenameAll);
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Overwrite"));
		connect(btn, &QPushButton::clicked, [=] {
			SendFileExistsAnswer(io::FileExistsAnswer::Overwrite);
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Overwrite All"));
		connect(btn, &QPushButton::clicked, [=] {
			SendFileExistsAnswer(io::FileExistsAnswer::OverwriteAll);
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Abort"));
		connect(btn, &QPushButton::clicked, [=] {
			SendFileExistsAnswer(io::FileExistsAnswer::Abort);
		});
		hlayout->addWidget(btn);
	}
	
	return pane;
}

void TaskGui::CreateGui()
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	
	stack_.layout = new QStackedLayout();
	setLayout(stack_.layout);
	
	stack_.progress_index = stack_.layout->addWidget(CreateProgressPane());
	adjustSize();
}

QWidget*
TaskGui::CreateProgressPane()
{
	QWidget *pane = new QWidget();
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
	pane->setLayout(layout);
	{
		QBoxLayout *horiz = new QBoxLayout(QBoxLayout::TopToBottom);
		layout->addLayout(horiz);
		
		progress_bar_ = new QProgressBar();
		horiz->addWidget(progress_bar_);
		progress_bar_->setRange(ProgressMin, ProgressMax);
		progress_bar_->setTextVisible(true);
		
		QBoxLayout *two_label_layout = new QBoxLayout(QBoxLayout::LeftToRight);
		horiz->addLayout(two_label_layout);
		
		speed_ = new QLabel();
		two_label_layout->addWidget(speed_);
		two_label_layout->addStretch();
		info_ = new QLabel();
		two_label_layout->addWidget(info_);
	}
	
	{
		auto state = task_->data().GetState(&task_question_);
		QIcon &icon = (state == io::TaskState::Continue) ?
			pause_icon_ : continue_icon_;
		play_pause_btn_ = new QToolButton();
		play_pause_btn_->setIcon(icon);
		connect(play_pause_btn_, &QToolButton::clicked, [=] {
			ProcessAction(actions::IOContinue);
		});
		layout->addWidget(play_pause_btn_);
	}
	{
		auto *btn = new QToolButton();
		btn->setIcon(QIcon::fromTheme(IconCancel));
		connect(btn, &QToolButton::clicked, [=] {
			ProcessAction(actions::IOCancel);
		});
		layout->addWidget(btn);
	}
	
	return pane;
}

QWidget*
TaskGui::CreateWriteFailedPane()
{
	QWidget *pane = new QWidget();
	auto *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	pane->setLayout(vert_layout);
	
	write_failed_list_.line_edit = new QLineEdit();
	write_failed_list_.line_edit->setReadOnly(true);
	vert_layout->addWidget(write_failed_list_.line_edit);
	
	QBoxLayout *hlayout = new QBoxLayout(QBoxLayout::LeftToRight);
	vert_layout->addLayout(hlayout);
	
	{
		auto *btn = new QPushButton(tr("Skip"));
		connect(btn, &QPushButton::clicked, [=] {
			SendWriteFailedAnswer(io::WriteFailedAnswer::Skip);
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Skip All"));
		connect(btn, &QPushButton::clicked, [=] {
			SendWriteFailedAnswer(io::WriteFailedAnswer::SkipAll);
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Retry"));
		connect(btn, &QPushButton::clicked, [=] {
			SendWriteFailedAnswer(io::WriteFailedAnswer::Retry);
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Abort"));
		connect(btn, &QPushButton::clicked, [=] {
			SendWriteFailedAnswer(io::WriteFailedAnswer::Abort);
		});
		hlayout->addWidget(btn);
	}
	
	return pane;
}

TaskGui*
TaskGui::From(io::Task *task, TasksWin *tw)
{
	auto *g = new TaskGui(task);
	g->tasks_win_ = tw;
	
	return g;
}

void
TaskGui::PresentUserFileExistsQuestion()
{
	if (stack_.file_exists_index == -1) {
		stack_.file_exists_index = stack_.layout->addWidget(CreateFileExistsPane());
	}
	stack_.layout->setCurrentIndex(stack_.file_exists_index);
	QString s = task_question_.explanation + QLatin1String(": ") +
		task_question_.file_path_in_question;
	
	file_exists_list_.line_edit->setText(s);

	setFocus();
	tasks_win_->setVisible(true);
}

void
TaskGui::PresentUserWriteFailedQuestion()
{
	if (stack_.write_failed_index == -1) {
		stack_.write_failed_index = stack_.layout->addWidget(CreateWriteFailedPane());
	}
	stack_.layout->setCurrentIndex(stack_.write_failed_index);
	QString s = task_question_.explanation + QLatin1String(": ") +
		task_question_.file_path_in_question;
	
	write_failed_list_.line_edit->setText(s);

	setFocus();
	tasks_win_->setVisible(true);
}

void
TaskGui::ProcessAction(const QString &action)
{
	auto &data = task_->data();
	if (action == actions::IOContinue) {
		auto state = data.GetState(nullptr);
		io::TaskState new_state;
		if (state & io::TaskState::Continue) {
			new_state = io::TaskState::Pause;
		} else if (state & io::TaskState::Pause) {
			new_state = io::TaskState::Continue;
		} else {
			mtl_trace();
		}
		data.ChangeState(new_state);
		TaskStateChanged(new_state);
		
	} else if (action == actions::IOCancel) {
		data.ChangeState(io::TaskState::Abort);
		timer_->start();
	}
}

//QSize TaskGui::minimumSizeHint() const { return sizeHint(); }

void
TaskGui::SendFileExistsAnswer(const io::FileExistsAnswer answer)
{
	auto &data = task_->data();
	task_question_.file_exists_answer = answer;
	data.ChangeState(io::TaskState::Answered, &task_question_);
	timer_->start();
}

void
TaskGui::SendWriteFailedAnswer(const io::WriteFailedAnswer answer)
{
	auto &data = task_->data();
	task_question_.write_failed_answer = answer;
	data.ChangeState(io::TaskState::Answered, &task_question_);
	timer_->start();
}

//QSize TaskGui::sizeHint() const {
//	int rh = fontMetrics().boundingRect("Aj").height();
//	return QSize(-1, rh * 3);
//}

void TaskGui::TaskStateChanged(const io::TaskState new_state)
{
	if (new_state & io::TaskState::Continue) {
		play_pause_btn_->setIcon(pause_icon_);
		timer_->start();
	} else if (new_state & io::TaskState::Pause) {
		play_pause_btn_->setIcon(continue_icon_);
	}
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
	speed_->setText(s);
}

}

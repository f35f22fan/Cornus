#include "TaskGui.hpp"

#include "actions.hxx"
#include "../CondMutex.hpp"
#include "../io/io.hh"
#include "../io/Task.hpp"
#include "TasksWin.hpp"

#include <QBoxLayout>
#include <QPushButton>
#include <QTimer>

namespace cornus::gui {
using io::TaskState;

static const auto TaskFinishedOrAborted = TaskState::Finished | TaskState::Abort;
const QString IconCancel = QLatin1String("edit-delete");
const int ProgressMin = 0;
const int ProgressMax = 100;

void Invoke(TaskGui *task_gui)
{
	QMetaObject::invokeMethod(task_gui, "CheckTaskState", Qt::QueuedConnection);
}

void* wait_for_signal(void *ptr)
{
	pthread_detach(pthread_self());
	
	TaskGui *task_gui = (TaskGui*)ptr;
	auto &data = task_gui->task()->data();
	bool first_time = true;
	do {
		auto g = data.cm.guard();
		if (data.state & TaskState::AwaitingAnswer)
		{
			Invoke(task_gui);
			const auto condition = TaskState::Answered | TaskFinishedOrAborted;
			// mtl_info("Waiting for %s", qPrintable(ToString(condition)));
			data.WaitFor(condition, Lock::No);
			// mtl_info("Waiting for %s ... Done", qPrintable(ToString(condition)));
			if (data.state & TaskState::Answered)
			{
				Invoke(task_gui);
			}
		} else {
			const auto condition = TaskState::AwaitingAnswer | TaskFinishedOrAborted | TaskState::Working;
			// mtl_info("Waiting for %s", qPrintable(ToString(condition)));
			data.WaitFor(condition, Lock::No);
			// mtl_info("Waiting for %s .. Done", qPrintable(ToString(data.state)));
			if (data.state & TaskState::Working)
			{
				// mtl_info("TaskState::Working (Sleep 300ms)");
				data.cm.Unlock();
				const useconds_t ms = first_time ? 1000 * 100 : 300 * 1000;
				usleep(ms);
				if (first_time)
					first_time = false;
				data.cm.Lock();
				Invoke(task_gui);
				continue;
			} else {
				Invoke(task_gui);
			}
		}
		
		cauto state = data.state;
		if (state & TaskFinishedOrAborted)
		{
			// mtl_info("Task done: %s", qPrintable(ToString(state)));
			Invoke(task_gui);
			break;
		}
	} while (true);
	
	return nullptr;
}

TaskGui::TaskGui(io::Task *task): task_(task)
{
	//mtl_printq2("TaskGui thread: ", io::thread_id_short(pthread_self()));
	io::NewThread(wait_for_signal, this);
}

TaskGui::~TaskGui()
{
	task_->data().WaitFor(TaskFinishedOrAborted);
	delete task_;
}

void TaskGui::CheckTaskState() {
	//	auto thread_ba = io::thread_id_short(pthread_self()).toLocal8Bit();
	//	mtl_info("Thread: %s", thread_ba.data());
	const TaskState state = task_->data().GetState(&task_question_);
	
	if (state & TaskFinishedOrAborted)
	{
		tasks_win_->TaskDone(this, state);
		return;
	}
	
	if (!gui_created_)
		CreateGui();
	
	made_visible_once_ = true;// don't show dialog unless an error occurred or user clicked sys tray.
	if (!made_visible_once_)
	{
		made_visible_once_ = true;
		tasks_win_->adjustSize();
		tasks_win_->setVisible(true);
	}
	
	if (state & TaskState::Working) {
//static int n = 0;
//mtl_info("TaskState::Working %d", n++);
		auto prev_id = progress_.details_id;
		task_->progress().Get(progress_);
		
		if (progress_.total != 0) /// checking==0 to avoid division by zero
		{
			UpdateSpeedLabel();
			int at = progress_.at / (progress_.total / 100);
			progress_bar_->setValue(at);
		}
		
		if (prev_id != progress_.details_id)
		{
			info_->setText(progress_.details);
		}
	}
	
	if (state & TaskState::AwaitingAnswer) {
		switch (task_question_.question) {
		case io::Question::FileExists: {
			PresentUserFileExistsQuestion();
			break;
		}
		case io::Question::WriteFailed: {
			mtl_trace("Write Failed");
			PresentUserWriteFailedQuestion();
			break;
		}
		case io::Question::DeleteFailed: {
			PresentUserDeleteFailedQuestion();
			break;
		}
		default: {
			mtl_trace();
			break;
		}
		}
	}

	if (state & TaskState::Continue)
	{
		auto &data = task_->data();
		auto state = data.GetState();
		// mtl_info("RESTORING PROGRESS DISPLAY PANE: %s", qPrintable(ToString(state)));
		
		stack_.layout->setCurrentIndex(stack_.progress_index);
		cauto new_state = TaskState::Working;
		data.ChangeState(new_state);
		UpdateStartPauseIcon(new_state);
	}
}

void TaskGui::ContinueOrPause()
{
	auto &data = task_->data();
	auto old_state = data.GetState(nullptr);
	io::TaskState new_state;
	if (old_state & TaskState::Working) {
		new_state = TaskState::Pause;
	} else if (old_state & TaskState::Pause) {
		new_state = TaskState::Continue | TaskState::Working;
	} else {
		mtl_trace();
		return;
	}
	
	UpdateStartPauseIcon(new_state);
	data.ChangeState(new_state);
}

QWidget* TaskGui::CreateDeleteFailedPane()
{
	QWidget *pane = new QWidget();
	auto *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	pane->setLayout(vert_layout);
	
	delete_failed_list_.line_edit = new QLineEdit();
	delete_failed_list_.line_edit->setReadOnly(true);
	vert_layout->addWidget(delete_failed_list_.line_edit);
	
	QBoxLayout *hlayout = new QBoxLayout(QBoxLayout::LeftToRight);
	vert_layout->addLayout(hlayout);
	
	{
		auto *btn = new QPushButton(tr("Skip"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.skip(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Skip All"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.skip_all(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Retry"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.retry(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Abort"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.abort(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	return pane;
}

QWidget* TaskGui::CreateFileExistsPane()
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
			answer_.skip(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Skip All"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.skip_all(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Auto Rename"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.auto_rename(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Auto Rename All"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.auto_rename_all(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Overwrite"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.overwrite(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Overwrite All"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.overwrite_all(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Abort"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.abort(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	return pane;
}

void TaskGui::CreateGui()
{
//	mtl_info("Created GUI");
	gui_created_ = true;
	continue_icon_ = QIcon::fromTheme(QLatin1String("media-playback-start"));
	pause_icon_ = QIcon::fromTheme(QLatin1String("media-playback-pause"));
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	
	stack_.layout = new QStackedLayout();
	setLayout(stack_.layout);
	stack_.progress_index = stack_.layout->addWidget(CreateProgressPane());
	adjustSize();
}

QWidget* TaskGui::CreateProgressPane()
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
		QIcon &icon = (state & TaskState::Working) ?
			pause_icon_ : continue_icon_;
		work_pause_btn_ = new QToolButton();
		work_pause_btn_->setIcon(icon);
		connect(work_pause_btn_, &QToolButton::clicked, [=] {
			ContinueOrPause();
		});
		layout->addWidget(work_pause_btn_);
	}
	{
		auto *btn = new QToolButton();
		btn->setIcon(QIcon::fromTheme(IconCancel));
		connect(btn, &QToolButton::clicked, [=] {
			auto &data = task_->data();
			data.ChangeState(io::TaskState::Abort);
		});
		layout->addWidget(btn);
	}
	
	return pane;
}

QWidget* TaskGui::CreateWriteFailedPane()
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
			answer_.skip(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Skip All"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.skip_all(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Retry"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.retry(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	{
		auto *btn = new QPushButton(tr("Abort"));
		connect(btn, &QPushButton::clicked, [=] {
			answer_.abort(true);
			SendAnswer();
		});
		hlayout->addWidget(btn);
	}
	
	return pane;
}

TaskGui* TaskGui::From(io::Task *task, TasksWin *tw)
{
	auto *g = new TaskGui(task);
	g->tasks_win_ = tw;
	
	return g;
}

void TaskGui::PresentUserDeleteFailedQuestion()
{
	if (stack_.delete_failed_index == -1) {
		stack_.delete_failed_index = stack_.layout->addWidget(CreateDeleteFailedPane());
	}
	stack_.layout->setCurrentIndex(stack_.delete_failed_index);
	QString s = tr("[Delete] ") + task_question_.explanation + QLatin1String(": ") +
		task_question_.file_path_in_question;
	
	delete_failed_list_.line_edit->setText(s);
	PresentWindow();
}

void TaskGui::PresentUserFileExistsQuestion()
{
	if (stack_.file_exists_index == -1) {
		stack_.file_exists_index = stack_.layout->addWidget(CreateFileExistsPane());
	}
	stack_.layout->setCurrentIndex(stack_.file_exists_index);
	QString s = tr("[File Exists] ") + task_question_.explanation + QLatin1String(": ") +
		task_question_.file_path_in_question;
	file_exists_list_.line_edit->setText(s);
	PresentWindow();
}

void TaskGui::PresentUserWriteFailedQuestion()
{
	if (stack_.write_failed_index == -1) {
		stack_.write_failed_index = stack_.layout->addWidget(CreateWriteFailedPane());
	}
	stack_.layout->setCurrentIndex(stack_.write_failed_index);
	const QString msg_str = tr("[Write Failed] ")
		+ task_question_.explanation + QLatin1String(": ")
		+ task_question_.file_path_in_question;
	
	write_failed_list_.line_edit->setText(msg_str);
	PresentWindow();
}

void TaskGui::PresentWindow()
{
	tasks_win_->setVisible(true);
	tasks_win_->setFocus();
	setFocus(Qt::MouseFocusReason);
}

void TaskGui::SendAnswer()
{
	task_->data().ChangeState(TaskState::Answered, &answer_);
	answer_.clear();
}

void TaskGui::UpdateSpeedLabel()
{
	if (progress_.time_worked < 1000) {
		/// if progress is less than 1 second then the speed
		/// counting algorithm simply doesn't work properly.
		return;
	}
	
	cf64 seconds = double(progress_.time_worked) / 1000;
	ci64 n = double(progress_.at) / seconds;
	QString s = io::SizeToString(n) + QLatin1String("/s");
	speed_->setText(s);
}

void TaskGui::UpdateStartPauseIcon(const io::TaskState new_state)
{
	if (new_state & TaskState::Working) {
		work_pause_btn_->setIcon(pause_icon_);
	} else if (new_state & TaskState::Pause) {
		work_pause_btn_->setIcon(continue_icon_);
	}
}

}

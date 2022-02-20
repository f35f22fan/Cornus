#pragma once

#include <QStackedLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QToolButton>
#include <QWidget>

#include "../err.hpp"
#include "../io/decl.hxx"
#include "../MutexGuard.hpp"
#include "../io/Task.hpp"

namespace cornus::gui {

class TaskGui: public QWidget {
	Q_OBJECT
public:
	~TaskGui();
	static TaskGui* From(io::Task *task, gui::TasksWin *tw);
	io::Task* task() const { return task_; }
	void UpdateStartPauseIcon(const io::TaskState new_state);
	
public Q_SLOTS:
	void CheckTaskState();
	
private:
	TaskGui(io::Task *task);
	
	void ContinueOrPause();
	QWidget* CreateDeleteFailedPane();
	QWidget* CreateFileExistsPane();
	QWidget* CreateWriteFailedPane();
	void CreateGui();
	QWidget* CreateProgressPane();
	void PresentUserDeleteFailedQuestion();
	void PresentUserFileExistsQuestion();
	void PresentUserWriteFailedQuestion();
	void PresentWindow();
	void SendDeleteFailedAnswer(const io::DeleteFailedAnswer answer);
	void SendFileExistsAnswer(const io::FileExistsAnswer answer);
	void SendWriteFailedAnswer(const io::WriteFailedAnswer answer);
	void UpdateSpeedLabel();
	
	NO_ASSIGN_COPY_MOVE(TaskGui);
	
	io::Task *task_ = nullptr;
	QProgressBar *progress_bar_ = nullptr;
	TasksWin *tasks_win_ = nullptr;
	QLabel *info_ = nullptr;
	QLabel *speed_ = nullptr;
	QToolButton *work_pause_btn_ = nullptr;
	cornus::io::Progress progress_ = {};
	QIcon continue_icon_, pause_icon_;
	io::TaskQuestion task_question_ = {};
	bool gui_created_ = false;
	bool made_visible_once_ = false;
	
	struct Stack {
		QStackedLayout *layout = nullptr;
		int progress_index = -1;
		int delete_failed_index = -1;
		int file_exists_index = -1;
		int write_failed_index = -1;
	} stack_ = {};
	
	struct FileExistsList {
		QLineEdit *line_edit = nullptr;
	} file_exists_list_ = {};
	
	struct WriteFailedList {
		QLineEdit *line_edit = nullptr;
	} write_failed_list_ = {};
	
	struct DeleteFailedList {
		QLineEdit *line_edit = nullptr;
	} delete_failed_list_ = {};
};
}

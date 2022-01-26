#pragma once

#include <QStackedLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QToolButton>
#include <QWidget>
#include <QTimer>

#include "../err.hpp"
#include "../io/decl.hxx"
#include "../MutexGuard.hpp"
#include "../io/Task.hpp"

namespace cornus::gui {

class TaskGui: public QWidget {
public:
	~TaskGui();
	static TaskGui* From(io::Task *task, gui::TasksWin *tw);
	
//	virtual QSize sizeHint() const override;
//	virtual QSize minimumSizeHint() const override;
	
	void CheckTaskState();
	void TaskStateChanged(const io::TaskState new_state);
	
private:
	TaskGui(io::Task *task);
	QWidget* CreateDeleteFailedPane();
	QWidget* CreateFileExistsPane();
	QWidget* CreateWriteFailedPane();
	void CreateGui();
	QWidget* CreateProgressPane();
	void PresentUserDeleteFailedQuestion();
	void PresentUserFileExistsQuestion();
	void PresentUserWriteFailedQuestion();
	void PresentWindow();
	void ProcessAction(const QString &action);
	void SendDeleteFailedAnswer(const io::DeleteFailedAnswer answer);
	void SendFileExistsAnswer(const io::FileExistsAnswer answer);
	void SendWriteFailedAnswer(const io::WriteFailedAnswer answer);
	void UpdateSpeedLabel();
	
	NO_ASSIGN_COPY_MOVE(TaskGui);
	
	io::Task *task_ = nullptr;
	QProgressBar *progress_bar_ = nullptr;
	TasksWin *tasks_win_ = nullptr;
	bool made_visible_once_ = false;
	QLabel *info_ = nullptr;
	QLabel *speed_ = nullptr;
	QTimer *timer_ = nullptr;
	QToolButton *work_pause_btn_ = nullptr;
	cornus::io::Progress progress_ = {};
	QIcon continue_icon_, pause_icon_;
	io::TaskQuestion task_question_ = {};
	
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

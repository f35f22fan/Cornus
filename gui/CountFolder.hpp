#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QTimer>

#include "../decl.hxx"
#include "../err.hpp"
#include "../io/io.hh"

namespace cornus::gui {

class CountFolder : public QDialog {
	Q_OBJECT
public:
	CountFolder(App *app, const QString &dir_path);
	~CountFolder();
	
	void CheckState();
	
public Q_SLOTS:
	void UpdateInfo(cornus::io::CountRecursiveInfo *info, const QString &err_msg);
	
private:
	NO_ASSIGN_COPY_MOVE(CountFolder);
	
	void CreateGui();
	bool Init(const QString &dir_path);
	
	App *app_ = nullptr;
	QTimer *timer_ = nullptr;
	
	QString full_path_, name_;
	QLineEdit *folder_name_label_ = nullptr;
	QLineEdit *size_label_ = nullptr;
	QLineEdit *size_no_meta_label_ = nullptr;
	QLineEdit *file_count_label_ = nullptr;
	QLineEdit *folder_count_label_ = nullptr;
	QLineEdit *progress_label_ = nullptr;
	
	io::CountFolderData *data_ = nullptr;
	bool thread_has_quit_ = false;
};
}

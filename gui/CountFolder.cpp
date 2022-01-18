#include "CountFolder.hpp"

#include "../App.hpp"
#include "../AutoDelete.hh"
#include "../io/io.hh"
#include "../MutexGuard.hpp"
#include "../trash.hh"

#include <QBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

namespace cornus::gui {

void *CountFolderTh(void *arg)
{
	pthread_detach(pthread_self());
	io::CountFolderData *data = (io::CountFolderData*)arg;
	bool is_trash_dir;
	const QString name = io::GetFileNameOfFullPath(data->full_path).toString();
	int err = io::DoStat(data->full_path, name, is_trash_dir, true, *data);
	
	if (err != 0){
		auto guard = data->guard();
		data->err_str = QObject::tr("Failed: ") + strerror(err);
		data->thread_has_quit = true;
		return nullptr;
	}
	
	err = io::CountSizeRecursiveTh(data->full_path, *data, is_trash_dir);
	
	auto guard = data->guard();
	if (data->app_has_quit)
	{
		delete data;
		return nullptr;
	}
	
	if (err != 0)
		data->err_str = QObject::tr("Failed: ") + strerror(err);
	
	data->thread_has_quit = true;
	
	return nullptr;
}

CountFolder::CountFolder(App *app, const QString &dir_path):
app_(app)
{
	if (!Init(dir_path))
		return;
	
	mtl_printq(dir_path);
	
	CreateGui();
	
	setWindowTitle(tr("Folder stats"));
	adjustSize();
	setModal(true);
	setVisible(true);
	
	data_ = new io::CountFolderData();
	data_->full_path = full_path_;
	
	pthread_t th;
	int status = pthread_create(&th, NULL, CountFolderTh, data_);
	if (status != 0) {
		mtl_status(status);
		return;
	}
	
	timer_ = new QTimer(this);
	connect(timer_, &QTimer::timeout, this,
		QOverload<>::of(&CountFolder::CheckState));
	timer_->start(500);
	
	exec();
}

CountFolder::~CountFolder()
{
	if (thread_has_quit_) {
		delete data_;
		data_ = nullptr;
	} else {
		MutexGuard mutex(&data_->mutex);
		data_->app_has_quit = true;
	}
}

void CountFolder::CheckState()
{
	io::CountRecursiveInfo info;
	QString error;
	{
		MutexGuard guard(&data_->mutex);
		info = data_->info;
		thread_has_quit_ = data_->thread_has_quit;
		error = data_->err_str;
	}
	UpdateInfo(&info, error);
}

const int FixedLineWidth = 500;

inline QLineEdit* CreateLineEdit() {
	QLineEdit *p = new QLineEdit();
	p->setReadOnly(true);
	p->setFixedWidth(FixedLineWidth);
	return p;
}

void CountFolder::CreateGui()
{
	QBoxLayout *vert_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(vert_layout);
	
	QFormLayout *form = new QFormLayout();
	vert_layout->addLayout(form);
	
	{
		folder_name_label_ = CreateLineEdit();
		int folder_name_w = FixedLineWidth;
		int w = folder_name_label_->fontMetrics().boundingRect(name_).width() + 20;
		if (w > FixedLineWidth)
			folder_name_w = std::min(FixedLineWidth + 200, w);
		folder_name_label_->setFixedWidth(folder_name_w);
		form->addRow(tr("Folder:"), folder_name_label_);
		folder_name_label_->setText(full_path_);
	}
	
	folder_count_label_ = CreateLineEdit();
	form->addRow(tr("Sub-folders:"), folder_count_label_);
	
	file_count_label_ = CreateLineEdit();
	form->addRow(tr("Files:"), file_count_label_);
	
	size_label_ = CreateLineEdit();
	form->addRow(tr("Total size:"), size_label_);
	
	progress_label_ = CreateLineEdit();
	form->addRow(tr("Progress:"), progress_label_);
	
	{
		QFrame *line = new QFrame(this);
		line->setFrameShape(QFrame::HLine); // Horizontal line
		line->setFrameShadow(QFrame::Sunken);
		line->setLineWidth(1);
		vert_layout->addWidget(line);
		
		// ok + cancel buttons row
		QDialogButtonBox *button_box = new QDialogButtonBox (QDialogButtonBox::Close);
		button_box->setCenterButtons(true);
		connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
		vert_layout->addWidget(button_box);
	}
}

bool CountFolder::Init(const QString &dir_path)
{
	QString in_full_path = dir_path;
	io::ExpandLinksInDirPath(in_full_path, full_path_);
	
	if (full_path_ == QLatin1String("/")) {
		app_->TellUser(tr("Computing total file size from / would be misleading"));
		return false;
	}
	
	name_ = io::GetFileNameOfFullPath(full_path_).toString();
	return !name_.isEmpty();
}

void CountFolder::UpdateInfo(cornus::io::CountRecursiveInfo *info,
	const QString &err_msg)
{
	QString s;
	{
		s = locale_.toString(info->dir_count);
		s.append(tr(", in trash: "));
		s.append(locale_.toString(info->trash_dir_count));
		folder_count_label_->setText(s);
	}
	{
		s = locale_.toString(info->file_count);
		s.append(tr(", in trash: "));
		s.append(locale_.toString(info->trash_file_count));
		file_count_label_->setText(s);
	}
	{
		const i64 n = info->size;
		QString s = io::SizeToString(n);
		if (n > 1023) {
			s += QLatin1String(" (") +
				QString::number(n) + QLatin1String(" bytes)");
		}
		s.append(tr(", trash: ") + io::SizeToString(info->trash_size));
		size_label_->setText(s);
	}
	
	if (thread_has_quit_) {
		timer_->stop();
		progress_label_->setText(tr("Done ") + QChar(0x2705));
	} else if (!err_msg.isEmpty()) {
		timer_->stop();
		progress_label_->setText(QChar(0x274C) + tr("Error: ") + err_msg);
	} else {
		progress_label_->setText(tr("In progress.."));
	}
}

} // namespace

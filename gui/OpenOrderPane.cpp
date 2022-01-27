#include "OpenOrderPane.hpp"

#include "../App.hpp"
#include "../ByteArray.hpp"
#include "../DesktopFile.hpp"
#include "../ExecInfo.hpp"
#include "../io/socket.hh"
#include "../prefs.hh"
#include "OpenOrderModel.hpp"
#include "OpenOrderTable.hpp"
#include "Tab.hpp"
#include "Table.hpp"

#include <QBoxLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>

namespace cornus::gui {

QString GetFullFilePathForMime(QString mime)
{
	QString save_dir = cornus::prefs::QueryMimeConfigDirPath();
	if (!save_dir.endsWith('/'))
		save_dir.append('/');
	
	const QString filename = mime.replace('/', '-');
	return save_dir + filename;
}

bool SortDesktopFiles(DesktopFile *a, DesktopFile *b)
{
	int n = a->GetName().toLower().compare(b->GetName().toLower());
	return n >= 0 ? false : true;
}

OpenOrderPane::OpenOrderPane(App *app, gui::Tab *tab):
QDialog(app), app_(app), tab_(tab)

{
	CreateGui();
	QueryData();
	TableSelectionChanged();
	setWindowTitle(tr("Customize opening order"));
	setModal(true);
	setVisible(true);
	adjustSize();
	exec();
}

OpenOrderPane::~OpenOrderPane()
{
	ClearData();
	for (auto *next: all_desktop_files_)
		delete next;
	
	all_desktop_files_.clear();
	delete custom_binary_;
	
	delete removed_tf_;
	removed_tf_ = nullptr;
}

void OpenOrderPane::AddSelectedCustomItem()
{
	QVariant v = add_custom_cb_->currentData();
	DesktopFile *p = (DesktopFile*)v.value<void*>();
	int index;
	if (ContainsDesktopFile(hide_vec_, p, &index)) {
		hide_vec_.remove(index);
	}
	
	model_->AppendItem(p->Clone());
	adjustSize();
	UpdateRemovedList();
}

void OpenOrderPane::AskAddCustomExecutable()
{
	QFileDialog dialog(app_);
	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setViewMode(QFileDialog::Detail);
	dialog.setDirectory(app_->tab()->current_dir());
	
	if (!dialog.exec())
		return;
	
	QStringList filenames = dialog.selectedFiles();
	if (filenames.isEmpty())
		return;
	
	const QString &full_path = filenames[0];
	QString ext = io::GetFileNameExtension(full_path).toString();
	ExecInfo info = app_->QueryExecInfo(full_path, ext);
	if (!info.is_elf() && !info.is_shell_script())
	{
		app_->TellUser(tr("The file you selected is not an executable"));
		return;
	} else if (!info.has_exec_bit()) {
		QString s = tr("The file's exec bit is not set.\n\nHint: Select the file and press Ctrl+E");
		app_->TellUser(s);
		return;
	}
	
	DesktopFile *p = DesktopFile::JustExePath(full_path);
	VOID_RET_IF(p, nullptr);
	model_->AppendItem(p);
}

void OpenOrderPane::ButtonClicked(QAbstractButton *btn)
{
	if (btn == button_box_->button(QDialogButtonBox::Ok)) {
		if (WasOrderModified())
			Save();
		this->close();
	} else if (btn == button_box_->button(QDialogButtonBox::Cancel)) {
		this->close();
	} else if (btn == button_box_->button(QDialogButtonBox::RestoreDefaults)) {
		RestoreDefaults();
	}
}

void OpenOrderPane::ClearData()
{
	for (auto *next: open_with_original_vec_)
		delete next;
	open_with_original_vec_.clear();
	
	for (auto *next: hide_vec_)
		delete next;
	hide_vec_.clear();
	
	model_->ClearData();
}

QWidget* OpenOrderPane::CreateAddingCustomItem()
{
	ByteArray ba;
	ba.set_msg_id(io::Message::SendAllDesktopFiles);
	const int fd = io::socket::Client();
	if (fd == -1 || !ba.Send(fd, CloseSocket::No))
	{
		mtl_trace();
		return nullptr;
	}
	
	ba.Clear();
	RET_IF(ba.Receive(fd), false, nullptr);
	
	while (ba.has_more()) {
		DesktopFile *p = DesktopFile::From(ba);
		ret_if(p, nullptr, nullptr);
		all_desktop_files_.append(p);
	}
	
	std::sort(all_desktop_files_.begin(), all_desktop_files_.end(),
		cornus::gui::SortDesktopFiles);
	
	add_custom_cb_ = new QComboBox();
	const QString two_points = QLatin1String("..");
	const int max_len = 40;
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	
	for (DesktopFile *p: all_desktop_files_)
	{
		QString s = p->GetName();
		QString generic = p->GetGenericName();
		
		if (!generic.isEmpty()) {
			s.append(QLatin1String(" ("));
			s.append(generic);
			s.append(')');
		}
		
		if (s.size() > max_len)
		{
			s = s.mid(0, max_len);
			s.append(two_points);
		}
		
		add_custom_cb_->addItem(p->CreateQIcon(env), s,
			QVariant::fromValue((void*)p));
	}
	
	QFrame *p = new QFrame();
	p->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
	p->setLineWidth(2);
	auto *layout = new QBoxLayout(QBoxLayout::LeftToRight);
	p->setLayout(layout);
	layout->addWidget(add_custom_cb_);
	
	{
		QPushButton *btn = new QPushButton();
		btn->setIcon(QIcon::fromTheme(QLatin1String("list-add")));
		btn->setText(tr("Add") + QString(" ↓"));
		btn->setToolTip(tr("Add to table below"));
		connect(btn, &QPushButton::clicked, this, &OpenOrderPane::AddSelectedCustomItem);
		
		layout->addWidget(btn);
	}
	
	{
		QPushButton *btn = new QPushButton();
		btn->setIcon(QIcon::fromTheme(QLatin1String("application-x-executable")));
		btn->setText(tr("Add custom..."));
		btn->setToolTip(tr("Add custom executable to table below..."));
		connect(btn, &QPushButton::clicked, this, &OpenOrderPane::AskAddCustomExecutable);
		
		layout->addWidget(btn);
	}
	
	return p;
}

QDialogButtonBox*
OpenOrderPane::CreateButtonsPane()
{
	auto *box = new QDialogButtonBox (QDialogButtonBox::RestoreDefaults
		| QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	
	{
		up_btn_ = new QPushButton();
		up_btn_->setText(tr("Move Up"));
		up_btn_->setIcon(QIcon::fromTheme(QLatin1String("go-up")));
		box->addButton(up_btn_, QDialogButtonBox::NoRole);
		connect(up_btn_, &QAbstractButton::clicked, [=] {
			MoveItem(Direction::Up);
		});
	}
	
	{
		down_btn_ = new QPushButton();
		down_btn_->setText(tr("Move Down"));
		down_btn_->setIcon(QIcon::fromTheme(QLatin1String("go-down")));
		box->addButton(down_btn_, QDialogButtonBox::NoRole);
		connect(down_btn_, &QAbstractButton::clicked, [=] {
			MoveItem(Direction::Down);
		});
	}
	
	{
		remove_btn_ = new QPushButton();
		remove_btn_->setText(tr("Hide"));
		remove_btn_->setIcon(QIcon::fromTheme(QLatin1String("list-remove")));
		box->addButton(remove_btn_, QDialogButtonBox::NoRole);
		connect(remove_btn_, &QAbstractButton::clicked, [=] {
			RemoveSelectedItem();
		});
	}
	
	connect(box, &QDialogButtonBox::clicked, this, &OpenOrderPane::ButtonClicked);
	
	return box;
}

void OpenOrderPane::CreateGui()
{
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(layout);
	
	const OpenWith& open_with = tab_->open_with();
	mime_ = app_->QueryMimeType(open_with.full_path);
	layout->addWidget(CreateAddingCustomItem());
	{
		removed_tf_ = new QLineEdit();
		removed_tf_->setReadOnly(true);
		layout->addWidget(removed_tf_);
	}
	{
		model_ = new OpenOrderModel(app_, this);
		table_ = new OpenOrderTable(app_, model_);
		model_->table(table_);
		mtl_warn("TBD: handle icon_view_ as well.");
		connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged,
			this, &OpenOrderPane::TableSelectionChanged);
		
		layout->addWidget(table_);
	}
	
	{
		button_box_ = CreateButtonsPane();
		layout->addWidget(button_box_);
	}
}

int OpenOrderPane::GetSelectedRowIndex()
{
	auto *sel_model = table_->selectionModel();
	auto list = sel_model->selectedIndexes();
	if (list.isEmpty())
		return -1;
	
	return list[0].row();
}

void OpenOrderPane::MoveItem(const Direction d)
{
	int row = GetSelectedRowIndex();
	if (row == -1)
		return;
	
	int new_index = (d == Direction::Down) ? row + 1 : row - 1;
	const int row_count = model_->rowCount();
	
	if (new_index < row_count && new_index >= 0) {
		auto &vec = model_->vec();
		vec.move(row, new_index);
		model_->UpdateRowRange(row, new_index);
		model_->SetSelectedRow(new_index, row);
	}
}

void OpenOrderPane::QueryData()
{
	ClearData();
	const OpenWith& open_with = tab_->open_with();
	QVector<DesktopFile*> show_vec;
	for (DesktopFile *next: open_with.show_vec) {
		open_with_original_vec_.append(next->Clone());
		show_vec.append(next->Clone());
	}
	
	for (DesktopFile *next: open_with.hide_vec) {
		hide_vec_.append(next->Clone());
	}
	
	model_->SetData(show_vec);
	UpdateRemovedList();
}

void OpenOrderPane::RemoveSelectedItem()
{
	int row = GetSelectedRowIndex();
	if (row == -1)
		return;
	DesktopFile *p = model_->vec()[row];
	model_->RemoveItem(row);
	if (p->is_desktop_file()) {
		hide_vec_.append(p);
	} else {
		delete p;
	}
	adjustSize();
	UpdateRemovedList();
}

void OpenOrderPane::RestoreDefaults()
{
	const QString question = tr("Restore defaults for <b>") +
	mime_ + QLatin1String("</b>?");
	auto answer = QMessageBox::question(app_, tr("Please confirm"),
		question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (answer != QMessageBox::Yes)
		return;
	
	auto full_path = GetFullFilePathForMime(mime_);
	auto ba = full_path.toLocal8Bit();
	remove(ba.data()); // don't return on error, QueryData must be executed.
	tab_->ReloadOpenWith(); // to clear cache
	QueryData();
	adjustSize();
}

void OpenOrderPane::Save()
{
	const QString full_path = GetFullFilePathForMime(mime_);
	QVector<DesktopFile*> &model_vec = model_->vec();
	ByteArray ba;
	
	for (DesktopFile *next: model_vec)
	{
		ba.add_i8(i8(Present::Yes));
		ba.add_string(next->GetId());
	}
	
	const auto info = DesktopFile::GetForMime(mime_);
	for (DesktopFile *next: hide_vec_)
	{
		if (next->Supports(mime_, info, app_->desktop()) != Priority::Ignore) {
			ba.add_i8(i8(Present::No));
			ba.add_string(next->GetId());
		} else {
			auto ba = next->GetId().toLocal8Bit();
			auto mime_ba = mime_.toLocal8Bit();
			mtl_info("Not including %s because it doesn't support "
				"\"%s\" anyway", ba.data(), mime_ba.data());
		}
	}
	
	VOID_RET_IF(io::WriteToFile(full_path, ba.data(), ba.size(), io::PostWrite::FSync), 0);
}

void OpenOrderPane::TableSelectionChanged()
{
	auto *sel_model = table_->selectionModel();
	auto list = sel_model->selectedIndexes();
	const bool enabled = !list.isEmpty();
	
	up_btn_->setEnabled(enabled);
	down_btn_->setEnabled(enabled);
	remove_btn_->setEnabled(enabled);
}

void OpenOrderPane::UpdateRemovedList()
{
	QString s;
	const int count = hide_vec_.size();
	const QString period = QLatin1String(", ");
	for (int i = 0; i < count; i++)
	{
		DesktopFile *next = hide_vec_[i];
		s += next->GetId();
		if (i < (count - 1))
			s += period;
	}
	
	const QString text = tr("Hidden (") + QString::number(count)
		+ QLatin1String("): ") + s;
	removed_tf_->setText(text);
}

bool OpenOrderPane::WasOrderModified() const
{
	const auto &model_vec = model_->vec();
	const int count = model_vec.size();
	if (count != open_with_original_vec_.size())
		return true;
	
	for (int i = 0; i < count; i++) {
		if (model_vec[i]->GetName() != open_with_original_vec_[i]->GetName())
			return true;
	}
	
	return false;
}

}



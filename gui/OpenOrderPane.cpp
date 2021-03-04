#include "OpenOrderPane.hpp"

#include "../App.hpp"
#include "../ByteArray.hpp"
#include "../DesktopFile.hpp"
#include "../ExecInfo.hpp"
#include "../io/socket.hh"
#include "../prefs.hh"
#include "OpenOrderModel.hpp"
#include "OpenOrderTable.hpp"
#include "Table.hpp"

#include <QBoxLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace cornus::gui {

bool SortDesktopFiles(DesktopFile *a, DesktopFile *b)
{
	int n = a->GetName().toLower().compare(b->GetName().toLower());
	return n >= 0 ? false : true;
}

OpenOrderPane::OpenOrderPane(App *app): app_(app), QDialog(app)
{
	CreateGui();
	QueryData();
	TableSelectionChanged();
	setWindowTitle(tr("Preference Order"));
	setModal(true);
	setVisible(true);
	adjustSize();
	exec();
}

OpenOrderPane::~OpenOrderPane()
{
	for (auto *next: open_with_original_vec_)
		delete next;
	for (auto *next: removed_vec_)
		delete next;
	for (auto *next: all_desktop_files_)
		delete next;
	
	open_with_original_vec_.clear();
	removed_vec_.clear();
	all_desktop_files_.clear();
	delete custom_binary_;
}

void OpenOrderPane::AddSelectedCustomItem()
{
	QVariant v = add_custom_cb_->currentData();
	DesktopFile *p = (DesktopFile*)v.value<void*>();
	const int index = DesktopFileIndex(removed_vec_, p->GetId(), p->type());
	if (index != -1) {
		removed_vec_.remove(index);
	}
	
	model_->AppendItem(p->Clone());
	adjustSize();
}

void OpenOrderPane::AskAddCustomExecutable()
{
	QFileDialog dialog(app_);
	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setViewMode(QFileDialog::Detail);
	dialog.setDirectory(app_->current_dir());
	
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
	CHECK_PTR_VOID(p);
	model_->AppendItem(p);
}

void OpenOrderPane::ButtonClicked(QAbstractButton *btn)
{
	if (btn == button_box_->button(QDialogButtonBox::Ok)) {
		if (WasOrderModified()) {
			Save();
		}
		
		this->close();
	} else if (btn == button_box_->button(QDialogButtonBox::Cancel)) {
		this->close();
	}
}

QWidget* OpenOrderPane::CreateAddingCustomItem()
{
	ByteArray ba;
	ba.set_msg_id(io::socket::MsgBits::SendAllDesktopFiles);
	const int fd = io::socket::Client();
	CHECK_TRUE_NULL((fd != -1));
	CHECK_TRUE_NULL(ba.Send(fd, false));
	
	ba.Clear();
	CHECK_TRUE_NULL(ba.Receive(fd));
	
	while (ba.has_more()) {
		DesktopFile *p = DesktopFile::From(ba);
		CHECK_TRUE_NULL((p != nullptr));
		all_desktop_files_.append(p);
	}
	
	std::sort(all_desktop_files_.begin(), all_desktop_files_.end(),
		cornus::gui::SortDesktopFiles);
	
	add_custom_cb_ = new QComboBox();
	const QString two_points = QLatin1String("..");
	const int max_len = 40;
	
	for (DesktopFile *p: all_desktop_files_) {
		
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
		
		add_custom_cb_->addItem(p->CreateQIcon(), s,
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
		btn->setText(tr("Add To List"));
		connect(btn, &QPushButton::clicked, this, &OpenOrderPane::AddSelectedCustomItem);
		
		layout->addWidget(btn);
	}
	
	{
		QPushButton *btn = new QPushButton();
		btn->setIcon(QIcon::fromTheme(QLatin1String("application-x-executable")));
		btn->setText(tr("Custom..."));
		btn->setToolTip(tr("Add Custom Executable..."));
		connect(btn, &QPushButton::clicked, this, &OpenOrderPane::AskAddCustomExecutable);
		
		layout->addWidget(btn);
	}
	
	return p;
}

void OpenOrderPane::CreateGui()
{
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom);
	setLayout(layout);
	
	const OpenWith& open_with = app_->table()->open_with();
	mime_ = app_->QueryMimeType(open_with.full_path);
	
	QLabel *msg_widget = new QLabel();
	layout->addWidget(msg_widget);
	msg_widget->setText(tr("Preference order for <b>") + mime_
		+ QLatin1String("</b>"));
	
	model_ = new OpenOrderModel(app_, this);
	table_ = new OpenOrderTable(app_, model_);
	model_->table(table_);
	connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &OpenOrderPane::TableSelectionChanged);
	
	layout->addWidget(table_);
	
	QWidget *w = CreateAddingCustomItem();
	if (w)
		layout->addWidget(w);
	else
		mtl_trace();
	
	button_box_ = new QDialogButtonBox (QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel);
	
	{
		auto *btn = new QPushButton();
		btn->setText(tr("Move Up"));
		btn->setIcon(QIcon::fromTheme(QLatin1String("go-up")));
		button_box_->addButton(btn, QDialogButtonBox::NoRole);
		connect(btn, &QAbstractButton::clicked, [=] {
			MoveItem(Direction::Up);
		});
		up_btn_ = btn;
	}
	
	{
		auto *btn = new QPushButton();
		btn->setText(tr("Move Down"));
		btn->setIcon(QIcon::fromTheme(QLatin1String("go-down")));
		button_box_->addButton(btn, QDialogButtonBox::NoRole);
		connect(btn, &QAbstractButton::clicked, [=] {
			MoveItem(Direction::Down);
		});
		down_btn_ = btn;
	}
	
	{
		auto *btn = new QPushButton();
		btn->setText(tr("Remove"));
		btn->setIcon(QIcon::fromTheme(QLatin1String("list-remove")));
		button_box_->addButton(btn, QDialogButtonBox::NoRole);
		connect(btn, &QAbstractButton::clicked, [=] {
			RemoveSelectedItem();
		});
		remove_btn_ = btn;
	}
	
	connect(button_box_, &QDialogButtonBox::clicked, this, &OpenOrderPane::ButtonClicked);
	layout->addWidget(button_box_);
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
	const OpenWith& open_with = app_->table()->open_with();
	QVector<DesktopFile*> model_vec;
	for (DesktopFile *next: open_with.add_vec) {
		open_with_original_vec_.append(next->Clone());
		model_vec.append(next->Clone());
	}
	
	for (DesktopFile *next: open_with.remove_vec) {
		removed_vec_.append(next->Clone());
	}
	
	model_->SetData(model_vec);
}

void OpenOrderPane::RemoveSelectedItem()
{
	int row = GetSelectedRowIndex();
	if (row == -1)
		return;
	DesktopFile *p = model_->vec()[row];
	model_->RemoveItem(row);
	if (p->is_desktop_file()) {
		removed_vec_.append(p);
	} else {
		delete p;
	}
	adjustSize();
}

void OpenOrderPane::Save()
{
	QString save_dir = cornus::prefs::QueryMimeConfigDirPath();
	if (!save_dir.endsWith('/'))
		save_dir.append('/');
	
	QVector<DesktopFile*> &model_vec = model_->vec();
	ByteArray ba;
	
	for (DesktopFile *next: model_vec)
	{
		ba.add_i8(i8(DesktopFile::Action::Add));
		ba.add_string(next->GetId());
	}
	const auto info = DesktopFile::GetForMime(mime_);
	for (DesktopFile *next: removed_vec_)
	{
		if (next->SupportsMime(mime_, info)) {
			ba.add_i8(i8(DesktopFile::Action::Remove));
			ba.add_string(next->GetId());
		} else {
			auto ba = next->GetId().toLocal8Bit();
			auto mime_ba = mime_.toLocal8Bit();
			mtl_info("Not including %s because it doesn't support "
				"\"%s\" anyway", ba.data(), mime_ba.data());
		}
	}
	
	QString filename = mime_.replace('/', '-');
	QString full_path = save_dir + filename;
	
	CHECK_TRUE_VOID((io::WriteToFile(full_path, ba.data(), ba.size()) == io::Err::Ok));
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



#include "OpenOrderPane.hpp"

#include "../App.hpp"
#include "../ByteArray.hpp"
#include "../DesktopFile.hpp"
#include "../io/socket.hh"
#include "../prefs.hh"
#include "OpenOrderModel.hpp"
#include "OpenOrderTable.hpp"
#include "Table.hpp"

#include <QBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace cornus::gui {

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

OpenOrderPane::~OpenOrderPane() {}

void
OpenOrderPane::ButtonClicked(QAbstractButton *btn)
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
	
	connect(button_box_, &QDialogButtonBox::clicked, this, &OpenOrderPane::ButtonClicked);
	layout->addWidget(button_box_);
}

void
OpenOrderPane::MoveItem(const Direction d)
{
	auto *sel_model = table_->selectionModel();
	auto list = sel_model->selectedIndexes();
	if (list.isEmpty())
		return;
	
	const int row_count = model_->rowCount();
	
	int row = list[0].row();
	int new_index = (d == Direction::Down) ? row + 1 : row - 1;
	
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
	for (DesktopFile *next: open_with.vec) {
		vec_.append(next->Clone());
		model_vec.append(next->Clone());
	}
	
	model_->SetData(model_vec);
	
//	ByteArray ba;
//	ba.set_msg_id(io::socket::MsgBits::SendOpenWithList);
//	ba.add_string(mime);
//	CHECK_TRUE_VOID(io::socket::SendSync(ba));
}

void OpenOrderPane::Save()
{
	QString save_dir = cornus::prefs::QueryMimeConfigDirPath();
	if (!save_dir.endsWith('/'))
		save_dir.append('/');
	
	QVector<DesktopFile*> &vec = model_->vec();
	ByteArray ba;
//	ba.add_i32(vec.size());
	
	for (DesktopFile *next: vec) {
		ba.add_i8(i8(next->type()));
		ba.add_string(next->GetId());
	}
	
	QString filename = mime_.replace('/', '-');
	QString full_path = save_dir + filename;
	
	CHECK_TRUE_VOID((io::WriteToFile(full_path, ba.data(), ba.size()) == io::Err::Ok));
}

void OpenOrderPane::TableSelectionChanged()
{
	auto *sel_model = table_->selectionModel();
	auto list = sel_model->selectedIndexes();
	
	bool enabled = !list.isEmpty();
	up_btn_->setEnabled(enabled);
	down_btn_->setEnabled(enabled);
}

bool OpenOrderPane::WasOrderModified() const
{
	const auto &model_vec = model_->vec();
	const int count = model_vec.size();
	if (count != vec_.size())
		return true;
	
	for (int i = 0; i < count; i++) {
		if (model_vec[i]->GetName() != vec_[i]->GetName())
			return true;
	}
	
	return false;
}

}



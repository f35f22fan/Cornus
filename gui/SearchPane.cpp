#include "SearchPane.hpp"

#include "../App.hpp"
#include "../io/File.hpp"
#include "../MutexGuard.hpp"
#include "SearchLineEdit.hpp"
#include "Table.hpp"
#include "TableModel.hpp"

#include <QBoxLayout>

namespace cornus::gui {

SearchPane::SearchPane(App *app): app_(app)
{
	CreateGui();
}

SearchPane::~SearchPane() {}

void SearchPane::ActionHide()
{
	DeselectAll();
	setVisible(false);
	select_row_ = -1;
	last_dir_id_ = -1;
	search_le_->SetFound(-1);
	app_->table()->setFocus();
}

void SearchPane::CreateGui()
{
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
	setLayout(layout);
	layout->setContentsMargins(0, 0, 2, 2);
	
	search_le_ = new SearchLineEdit();
	search_le_->setPlaceholderText(tr("Search..."));
	search_le_->installEventFilter(this);
	connect(search_le_, &QLineEdit::textChanged, this, &SearchPane::TextChanged);
	layout->addWidget(search_le_);
	
	{
		case_sensitive_ = new QCheckBox();
		case_sensitive_->setText("Case Sensitive");
		layout->addWidget(case_sensitive_);
		connect(case_sensitive_, &QCheckBox::toggled, [this] {
			this->TextChanged(search_le_->text());
		});
	}
	
	{
		auto *btn = new QPushButton();
		btn->setIcon(QIcon::fromTheme(QLatin1String("go-previous")));
		connect(btn, &QPushButton::clicked, [=] {
			ScrollToNext(Direction::Prev);
		});
		layout->addWidget(btn);
	}
	{
		auto *btn = new QPushButton();
		btn->setIcon(QIcon::fromTheme(QLatin1String("go-next")));
		connect(btn, &QPushButton::clicked, [=] {
			ScrollToNext(Direction::Next);
		});
		layout->addWidget(btn);
	}
	{
		auto *btn = new QPushButton();
		btn->setIcon(QIcon::fromTheme(QLatin1String("window-close")));
		connect(btn, &QPushButton::clicked, this, &SearchPane::ActionHide);
		layout->addWidget(btn);
	}
}

void SearchPane::DeselectAll()
{
	QVector<int> indices;
	{
		auto &files = app_->view_files();
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		int i = 0;
		const auto bits = io::FileBits::SelectedBySearch | io::FileBits::SelectedBySearchActive;
		for (io::File *next: vec) {
			if (next->has(bits)) {
				next->toggle_flag(bits, false);
				indices.append(i);
			}
			i++;
		}
	}
	
	app_->table()->model()->UpdateIndices(indices);
}

bool SearchPane::eventFilter(QObject  *obj, QEvent * event)
{
	if((QLineEdit *)obj == search_le_ && event->type()== QEvent::KeyPress)
	{
		QKeyEvent *kevt = (QKeyEvent*)event;
		auto key = kevt->key();
		const bool ctrl = kevt->modifiers() & Qt::ControlModifier;
		
		switch (key) {
		case Qt::Key_Escape: {
			ActionHide();
			break;
		}
		case Qt::Key_Return: {
			ScrollToNext(ctrl ? Direction::Prev : Direction::Next);
			break;
		}
		} /// switch()
	}
	
	return false;
}

void SearchPane::RequestFocus()
{
	search_le_->setFocus();
	search_le_->selectAll();
	TextChanged(search_le_->text());
}

void SearchPane::ScrollToNext(const Direction dir)
{
	if (last_dir_id_ != app_->current_dir_id()) {
		last_dir_id_ = app_->current_dir_id();
		TextChanged(search_le_->text());
		return;
	}
	
	QVector<int> indices;
	{
		auto &files = app_->view_files();
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		
		io::File *active = nullptr;
		io::File *deactive = nullptr;
		
		if (dir == Direction::Next)
		{
			int i = (select_row_ == -1) ? 0 : select_row_;
			const int count = vec.size();
			for (; i < count; i++) {
				io::File *next = vec[i];
				if (next->selected_by_search())
				{
					if (select_row_ < i) {
						next->selected_by_search_active(true);
						active = next;
						select_row_ = i;
						indices.append(i);
						break;
					} else if (next->selected_by_search_active()) {
						next->selected_by_search_active(false);
						deactive = next;
						indices.append(i);
					}
				}
			}
		} else {
			int i = (select_row_ == -1) ? vec.size() - 1 : select_row_;
			for (; i >= 0; i--)
			{
				io::File *next = vec[i];
				if (next->selected_by_search())
				{
					if (select_row_ > i) {
						select_row_ = i;
						next->selected_by_search_active(true);
						active = next;
						indices.append(i);
						break;
					} else if (next->selected_by_search_active()){
						next->selected_by_search_active(false);
						deactive = next;
						indices.append(i);
					}
				}
			}
		}
		
		if (active == nullptr && deactive != nullptr) {
			/// undo if next/prev not found
			deactive->selected_by_search_active(true);
		}
	}
	
	app_->table()->model()->UpdateIndices(indices);
	if (select_row_ != -1)
		app_->table()->ScrollToRow(select_row_);
}

void SearchPane::TextChanged(const QString &s)
{
	QString search = s.trimmed().toLower();
	if (search.isEmpty()) {
		search_le_->SetFound(-1);
		DeselectAll();
		return;
	}
	
	last_dir_id_ = app_->current_dir_id();
	select_row_ = -1;
	i32 found = 0;
	QVector<int> indices;
	{
		auto &files = app_->view_files();
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		int i = 0;
		for (io::File *next: vec) {
			const QString &s = lower() ? next->name_lower() : next->name();
			if (s.indexOf(search) != -1) {
				found++;
				if (!next->selected_by_search()) {
					next->selected_by_search(true);
					indices.append(i);
				}
				if (select_row_ == -1) {
					next->selected_by_search_active(true);
					select_row_ = i;
					indices.append(i);
				}
			} else if (next->selected_by_search()) {
				next->selected_by_search(false);
				next->selected_by_search_active(false);
				indices.append(i);
			}
			i++;
		}
	}
	
	search_le_->SetFound(found);
	app_->table()->model()->UpdateIndices(indices);
	if (select_row_ != -1)
		app_->table()->ScrollToRow(select_row_);
}

}



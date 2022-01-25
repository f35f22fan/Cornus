#include "SearchPane.hpp"

#include "../App.hpp"
#include "../Hid.hpp"
#include "../io/File.hpp"
#include "../io/Files.hpp"
#include "Location.hpp"
#include "../Media.hpp"
#include "../MutexGuard.hpp"
#include "SearchLineEdit.hpp"
#include "Tab.hpp"
#include "Table.hpp"
#include "TableModel.hpp"
#include "TextField.hpp"

#include <QBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSet>

namespace cornus::gui {

bool Same(const MediaSearch &a, const MediaSearch &b)
{
	return a.actor == b.actor && a.director == b.director &&
	a.writer == b.writer && a.genre == b.genre &&
	a.subgenre == b.subgenre && a.country == b.country &&
	a.year == b.year;
}

SearchPane::SearchPane(App *app): app_(app)
{}

SearchPane::~SearchPane() {}

void SearchPane::ActionHide()
{
	setVisible(false);
	BeforeExiting();
	DeselectAll();
	select_row_ = -1;
	last_dir_id_ = -1;
	if (search_le_ != nullptr)
		search_le_->SetCount(-1);
	app_->tab()->table()->setFocus();
}

void SearchPane::BeforeExiting()
{
	if (select_row_ < 0 || last_dir_id_ != app_->current_dir_id())
		return;
	
	app_->hid()->SelectFileByIndex(app_->tab(), select_row_, DeselectOthers::Yes);
}

bool SearchPane::ContainsAll(const media::ShortData &data)
{
	if (media_search_.actor != -1)
	{
		if (!data.actors.contains(media_search_.actor))
			return false;
	}
	if (media_search_.director != -1)
	{
		if (!data.directors.contains(media_search_.director))
			return false;
	}
	if (media_search_.writer != -1)
	{
		if (!data.writers.contains(media_search_.writer))
			return false;
	}
	if (media_search_.genre != -1)
	{
		if (!data.genres.contains(media_search_.genre))
			return false;
	}
	if (media_search_.subgenre != -1)
	{
		if (!data.subgenres.contains(media_search_.subgenre))
			return false;
	}
	if (media_search_.country != -1)
	{
		if (!data.countries.contains(media_search_.country))
			return false;
	}
	if (media_search_.video_codec != -1)
	{
		if (!data.video_codecs.contains(media_search_.video_codec))
			return false;
	}
	const i16 y = media_search_.year;
	if (y != -1)
	{
		if (data.year_end != -1) {
			if (data.year > y || data.year_end < y) {
				return false;
			}
		} else if (data.year != y) {
			return false;
		}
	}
	
	return true;
}

QWidget* SearchPane::CreateByFileNamePane()
{
	QWidget *p = new QWidget();
	p->setContentsMargins(0, 0, 0, 0);
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
	p->setLayout(layout);
	layout->setContentsMargins(0, 0, 2, 2);
	
	search_le_ = new SearchLineEdit();
	search_le_->setPlaceholderText(tr("Search..."));
	search_le_->installEventFilter(this);
	connect(search_le_, &QLineEdit::textChanged, this, &SearchPane::TextChanged);
	layout->addWidget(search_le_);
	
	{
		case_sensitive_ = new QCheckBox();
		case_sensitive_->setText(tr("Case Sensitive"));
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
	
	return p;
}

QWidget* CreateRow3(QWidget *a, QWidget *b, QWidget *c)
{
	QWidget *pane = new QWidget();
	pane->setContentsMargins(0, 0, 0, 0);
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	pane->setLayout(layout);
	layout->addWidget(a);
	layout->addWidget(b);
	layout->addWidget(c);
	layout->addStretch(2);
	
	return pane;
}

QComboBox* CreateCB(Media *media, const media::Field f, const int w)
{
	QComboBox *p = new QComboBox();
	media->FillInNTS(p, f, Media::Fill::AddNoneOption);
	p->setFixedWidth(w);
	return p;
}

QWidget* SearchPane::CreateByMediaXattrPane()
{
	media_items_.media_xattr = new QWidget();
	QWidget *p = media_items_.media_xattr;
	p->setFocusPolicy(Qt::StrongFocus);
	p->installEventFilter(this);
	
	QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom);
	p->setLayout(layout);
	
	const int a_size = fontMetrics().boundingRect("a").width();
	const int fixed_w = a_size * 20;
	using media::Field;
	Media *media = app_->media();
	if (!media->loaded())
		cornus::media::Reload(app_);
	connect(media, &cornus::Media::Changed, [=] { MediaFileWasUpdated(); });
	
	{
		auto g = media->guard();
		media_items_.actors_cb = CreateCB(media, Field::Actors, fixed_w);
		media_items_.directors_cb = CreateCB(media, Field::Directors, fixed_w);
		media_items_.writers_cb = CreateCB(media, Field::Writers, fixed_w);
		media_items_.genres_cb = CreateCB(media, Field::Genres, fixed_w);
		media_items_.subgenres_cb = CreateCB(media, Field::Subgenres, fixed_w);
		media_items_.countries_cb = CreateCB(media, Field::Countries, fixed_w);
		media_items_.video_codec_cb = CreateCB(media, Field::VideoCodec, fixed_w);
	}
	
	media_items_.year_tf = new TextField();
	media_items_.year_tf->setFixedWidth(fixed_w);
	media_items_.year_tf->setPlaceholderText("Year");
	
	QWidget *search_pane = new QWidget();
	{
		search_pane->setContentsMargins(0, 0, 0, 0);
		QBoxLayout *layout = new QBoxLayout(QBoxLayout::LeftToRight);
		layout->setContentsMargins(0, 0, 0, 0);
		search_pane->setLayout(layout);
		media_items_.search_prev = new QPushButton();
		media_items_.search_prev->setIcon(QIcon::fromTheme(QLatin1String("go-previous")));
		media_items_.search_prev->setToolTip(tr("Search backwards"));
		connect(media_items_.search_prev, &QPushButton::clicked, [=] {
			ScrollToNext(Direction::Prev);
		});
		media_items_.search_next = new QPushButton(tr("Search"));
		media_items_.search_next->setIcon(QIcon::fromTheme(QLatin1String("go-next")));
		media_items_.search_next->setToolTip(tr("Search forward"));
		connect(media_items_.search_next, &QPushButton::clicked, [=] {
			ScrollToNext(Direction::Next);
		});
		layout->addWidget(media_items_.search_prev);
		layout->addWidget(media_items_.search_next);
		
		QPushButton *close_btn = new QPushButton();
		close_btn->setIcon(QIcon::fromTheme(QLatin1String("window-close")));
		connect(close_btn, &QPushButton::clicked, [=] {
			ActionHide();
		});
		layout->addWidget(close_btn);
		layout->addStretch(2);
	}
	
	layout->addWidget(CreateRow3(media_items_.actors_cb,
		media_items_.directors_cb, media_items_.writers_cb));
	layout->addWidget(CreateRow3(media_items_.genres_cb,
		media_items_.subgenres_cb, media_items_.countries_cb));
	layout->addWidget(CreateRow3(media_items_.video_codec_cb,
		media_items_.year_tf, search_pane));
	layout->addStretch(2);
	
	return p;
}

void SearchPane::DeselectAll()
{
	gui::Tab *tab = app_->tab();
	QSet<int> indices;
	{
		auto &files = *app_->files(tab->files_id());
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		int i = 0;
		const auto bits = io::FileBits::SelectedBySearch | io::FileBits::SelectedBySearchActive;
		for (io::File *next: vec) {
			if (next->has(bits)) {
				next->toggle_flag(bits, false);
				indices.insert(i);
			}
			i++;
		}
	}
	
	tab->table()->model()->UpdateIndices(indices);
}

void SearchPane::DoSearch(const QString *search_str)
{
	gui::Tab *tab = app_->tab();
	last_dir_id_ = app_->current_dir_id();
	
	if (search_by_ == SearchBy::MediaXAttrs)
		FillInSearchItem(media_search_);
	
	select_row_ = -1;
	i32 found = 0, at = 0;
	bool found_current = false;
	QSet<int> indices;
	{
		auto &files = *app_->files(tab->files_id());
		MutexGuard guard = files.guard();
		auto &vec = files.data.vec;
		int i = 0;
		for (io::File *next: vec) {
			if (Matches(next, search_str)) {
				found++;
				if (!found_current)
					at++;
				if (!next->selected_by_search()) {
					next->selected_by_search(true);
					indices.insert(i);
				}
				if (select_row_ == -1) {
					found_current = true;
					next->selected_by_search_active(true);
					select_row_ = i;
					indices.insert(i);
				}
			} else if (next->selected_by_search()) {
				next->selected_by_search(false);
				next->selected_by_search_active(false);
				indices.insert(i);
			}
			i++;
		}
	}
	
	if (search_by_ == SearchBy::FileName) {
		search_le_->SetCount(found);
		search_le_->SetAt(at, true);
	}
	tab->table_model()->UpdateIndices(indices);
	if (select_row_ != -1)
		tab->table()->ScrollToFile(select_row_);
}

bool SearchPane::eventFilter(QObject  *obj, QEvent * event)
{
	if (event->type()== QEvent::KeyPress)
	{
		QKeyEvent *kevt = (QKeyEvent*)event;
		auto key = kevt->key();
		const bool ctrl = kevt->modifiers() & Qt::ControlModifier;
		
		if ((QLineEdit *)obj == search_le_ || (QWidget*)obj == media_items_.media_xattr)
		{
			switch (key) {
			case Qt::Key_Escape: {
				ActionHide();
				break;
			}
			case Qt::Key_Return: {
				ScrollToNext(ctrl ? Direction::Prev : Direction::Next);
				break;
			}
			}
		}
	}
	
	return false;
}

void SearchPane::FillInSearchItem(MediaSearch &d)
{
	d.actor = media_items_.actors_cb->currentData().toInt();
	d.director = media_items_.directors_cb->currentData().toInt();
	d.writer = media_items_.writers_cb->currentData().toInt();
	
	d.genre = media_items_.genres_cb->currentData().toInt();
	d.subgenre = media_items_.subgenres_cb->currentData().toInt();
	d.country = media_items_.countries_cb->currentData().toInt();
	d.video_codec = media_items_.video_codec_cb->currentData().toInt();
	
	QString s = media_items_.year_tf->text();
	bool ok;
	i16 year = s.toInt(&ok);
	d.year = ok ? year : -1;
}

bool SearchPane::Matches(io::File *file, const QString *search_str)
{
	if (search_by_ == SearchBy::FileName)
	{
		RET_IF(search_str, nullptr, false);
		const QString &s = lower() ? file->name_lower() : file->name();
		return s.contains(*search_str);
	} else if (search_by_ == SearchBy::MediaXAttrs) {
		media::ShortData *data = file->media_attrs_decoded();
		if (data == nullptr)
			return false;
		
		return ContainsAll(*data);
	} else {
		mtl_trace();
	}
	
	return false;
}

void ReloadCombo(QComboBox *p, Media *media, const media::Field f)
{
	QVariant v = p->currentData();
	p->clear();
	media->FillInNTS(p, f, Media::Fill::AddNoneOption);
	int index = p->findData(v);
	if (index != -1)
		p->setCurrentIndex(index);
}

void SearchPane::MediaFileWasUpdated()
{
	Media *media = app_->media();
	{
		auto g = media->guard();
		ReloadCombo(media_items_.actors_cb, media, media::Field::Actors);
		ReloadCombo(media_items_.directors_cb, media, media::Field::Directors);
		ReloadCombo(media_items_.writers_cb, media, media::Field::Writers);
		ReloadCombo(media_items_.genres_cb, media, media::Field::Genres);
		ReloadCombo(media_items_.subgenres_cb, media, media::Field::Subgenres);
		ReloadCombo(media_items_.countries_cb, media, media::Field::Countries);
		ReloadCombo(media_items_.video_codec_cb, media, media::Field::VideoCodec);
	}
}

void SearchPane::RequestFocus()
{
	if (search_by_ == SearchBy::FileName) {
		if (search_le_ != nullptr) {
			search_le_->setFocus();
			search_le_->selectAll();
			TextChanged(search_le_->text());
		}
	} else if (search_by_ == SearchBy::MediaXAttrs) {
		if (media_items_.media_xattr != nullptr) {
			media_items_.media_xattr->setFocus();
		}
	} else {
		mtl_trace();
	}
}

void SearchPane::ScrollToNext(const Direction dir)
{
	bool searched = false;
	if (search_by_ == SearchBy::MediaXAttrs) {
		MediaSearch now;
		FillInSearchItem(now);
		if (now.isEmpty()) {
			app_->TellUser(tr("No search parameters specified"));
			return;
		}
		if (!Same(now, media_search_)) {
			DoSearch(nullptr);
			searched = true;
		}
	}
	
	if (last_dir_id_ != app_->current_dir_id()) {
		last_dir_id_ = app_->current_dir_id();
		if (search_by_ == SearchBy::FileName) {
			TextChanged(search_le_->text());
		} else if (search_by_ == SearchBy::MediaXAttrs) {
			if (!searched)
				DoSearch(nullptr);
		} else {
			mtl_trace();
		}
		return;
	}
	
	i32 at = 0;
	gui::Tab *tab = app_->tab();
	QSet<int> indices;
	{
		auto &files = *app_->files(tab->files_id());
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
						indices.insert(i);
						break;
					} else if (next->selected_by_search_active()) {
						next->selected_by_search_active(false);
						deactive = next;
						indices.insert(i);
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
						indices.insert(i);
						break;
					} else if (next->selected_by_search_active()){
						next->selected_by_search_active(false);
						deactive = next;
						indices.insert(i);
					}
				}
			}
		}
		
		if (active == nullptr && deactive != nullptr) {
			/// undo if next/prev not found
			deactive->selected_by_search_active(true);
		}
		
		for (int i = 0; i <= select_row_; i++) {
			io::File *next = vec[i];
			if (next->selected_by_search())
				at++;
		}
	}
	
	if (search_by_ == SearchBy::FileName)
		search_le_->SetAt(at, true);
	
	tab->table()->model()->UpdateIndices(indices);
	
	if (select_row_ != -1)
		tab->table()->ScrollToFile(select_row_);
}

void SearchPane::SetMode(const SearchBy mode)
{
	search_by_ = mode;
	if (search_by_ == SearchBy::FileName) {
		if (media_items_.by_name == -1)
			media_items_.by_name = addWidget(CreateByFileNamePane());
		setCurrentIndex(media_items_.by_name);
		setMaximumSize(50000, search_le_->size().height() * 1.2);
	} else if (search_by_ == SearchBy::MediaXAttrs) {
		if (media_items_.by_media_xattr == -1)
			media_items_.by_media_xattr = addWidget(CreateByMediaXattrPane());
		/** For some reason querying a local widget's height doesn't work,
			so using one from main's window. */
		const int wh = app_->location()->size().height();
		const int h = wh * 3 * 1.2;
		setMaximumSize(50000, h);
		setCurrentIndex(media_items_.by_media_xattr);
	} else {
		mtl_trace();
	}
}

void SearchPane::TextChanged(const QString &s)
{
	QString search = s.trimmed().toLower();
	if (search.isEmpty()) {
		search_le_->SetCount(-1);
		DeselectAll();
		return;
	}
	
	DoSearch(&search);
}

}

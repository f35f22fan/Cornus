#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"
#include "../media.hxx"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>

namespace cornus::gui {

struct MediaSearch {
	i32 actor = -1;
	i32 director = -1;
	i32 writer = -1;
	i16 genre = -1;
	i16 subgenre = -1;
	i16 year = -1;
	i16 country = -1;
	i16 video_codec = -1;
	bool isEmpty() const
	{
		return actor == -1 && director == -1 && writer == -1 &&
		genre == -1 && subgenre == -1 && year == -1 && country == -1 &&
		video_codec == -1;
	}
};

bool Same(const MediaSearch &a, const MediaSearch &b);

class SearchPane: public QStackedWidget {
enum class SearchBy : i8 {
	FileName,
	MediaXAttrs,
};
public:
	SearchPane(App *app);
	virtual ~SearchPane();
	
	void RequestFocus();
	void SetMode(const SearchBy mode);
	void SetSearchByFileName() { SetMode(SearchBy::FileName); }
	void SetSearchByMediaXattr() { SetMode(SearchBy::MediaXAttrs); }
	void TextChanged(const QString &s);
	void MediaFileWasUpdated();
	
protected:
	virtual bool eventFilter(QObject  *obj, QEvent * event) override;
private:
	NO_ASSIGN_COPY_MOVE(SearchPane);
	
	void ActionHide();
	void BeforeExiting();
	bool ContainsAll(const media::ShortData &data);
	void DeselectAll();
	void DoSearch(const QString *search_str);
	void FillInSearchItem(MediaSearch &d);
	bool lower() const { return !case_sensitive_->isChecked(); }
	bool Matches(io::File *file, const QString *search_str);
	void ScrollToNext(const Direction dir);
	QWidget* CreateByFileNamePane();
	QWidget* CreateByMediaXattrPane();
	
	App *app_ = nullptr;
	QCheckBox *case_sensitive_ = nullptr;
	SearchLineEdit *search_le_ = nullptr;
	struct MediaItems {
		int by_name = -1;
		int by_media_xattr = -1;
		QWidget *media_xattr = nullptr;
		QComboBox *actors_cb = nullptr,
		*directors_cb = nullptr,
		*writers_cb = nullptr,
		*genres_cb = nullptr,
		*subgenres_cb = nullptr,
		*years_cb = nullptr,
		*countries_cb = nullptr,
		*video_codec_cb = nullptr;
		TextField *year_tf = nullptr;
		QPushButton *search_prev = nullptr, *search_next = nullptr;
	} media_items_ = {};
	
	MediaSearch media_search_ = {};
	int select_row_ = -1;
	i32 last_dir_id_ = -1;
	SearchBy search_by_ = SearchBy::FileName;
	
};
}

#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"

#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>

namespace cornus::gui {
class SearchPane: public QWidget {
public:
	SearchPane(App *app);
	virtual ~SearchPane();
	
	void RequestFocus();
	void TextChanged(const QString &s);
	
protected:
	virtual bool eventFilter(QObject  *obj, QEvent * event) override;
	
private:
	NO_ASSIGN_COPY_MOVE(SearchPane);
	
	void ActionHide();
	void CreateGui();
	bool lower() const { return !case_sensitive_->isChecked(); }
	void DeselectAll();
	void ScrollToNext(const Direction dir);
	
	App *app_ = nullptr;
	QCheckBox *case_sensitive_ = nullptr;
	SearchLineEdit *search_le_ = nullptr;
	int select_row_ = -1;
	i32 last_dir_id_ = -1;
};
}

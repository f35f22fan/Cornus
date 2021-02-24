#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../err.hpp"

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
	void DeselectAll();
	void ScrollToNext(const Direction dir);
	
	App *app_ = nullptr;
	QLineEdit *search_le_ = nullptr;
	int select_row_ = -1;
	QString last_current_dir_;
};
}

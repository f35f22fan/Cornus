#pragma once

#include "../err.hpp"
#include "decl.hxx"

#include <QLineEdit>

namespace cornus::gui {
class SearchLineEdit: public QLineEdit {
public:
	SearchLineEdit(QWidget *parent = nullptr);
	virtual ~SearchLineEdit();
	
	void SetFound(const i32 n) { found_ = n; repaint(); }
	
protected:
	virtual void paintEvent(QPaintEvent *evt) override;

private:
	NO_ASSIGN_COPY_MOVE(SearchLineEdit);
	i32 found_ = -1;
};
}

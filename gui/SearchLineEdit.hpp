#pragma once

#include "../err.hpp"
#include "decl.hxx"

#include <QLineEdit>

namespace cornus::gui {
class SearchLineEdit: public QLineEdit {
public:
	SearchLineEdit(QWidget *parent = nullptr);
	virtual ~SearchLineEdit();
	
	i4 count() const { return count_; }
	void SetCount(const i4 count) { count_ = count; }
	void SetAt(const i4 at, const bool do_repaint) {
		at_ = at;
		if (do_repaint)
			repaint();
	}
	
protected:
	virtual void paintEvent(QPaintEvent *evt) override;

private:
	NO_ASSIGN_COPY_MOVE(SearchLineEdit);
	i4 count_ = -1;
	i4 at_ = -1;
};
}

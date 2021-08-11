#pragma once

#include "decl.hxx"
#include "../err.hpp"

#include <QHeaderView>

namespace cornus::gui {
class TableHeader: public QHeaderView {
public:
	TableHeader(Table *parent);
	virtual ~TableHeader();
	
	bool in_drag_mode() const { return in_drag_mode_; }

protected:
	virtual void dragEnterEvent(QDragEnterEvent *evt) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *evt) override;
	virtual void dragMoveEvent(QDragMoveEvent *evt) override;
	virtual void dropEvent(QDropEvent *event) override;
	
private:
	void RepaintName();
	
	Table *table_ = nullptr;
	bool in_drag_mode_ = false;

};
}

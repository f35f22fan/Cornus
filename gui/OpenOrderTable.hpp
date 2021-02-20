#pragma once

#include <QTableView>

#include "../err.hpp"
#include "../decl.hxx"
#include "decl.hxx"

namespace cornus::gui {
class OpenOrderTable: public QTableView {
public:
	OpenOrderTable(App *app, OpenOrderModel *model);
	virtual ~OpenOrderTable();
	
	virtual QSize sizeHint() const;
	
private:
	NO_ASSIGN_COPY_MOVE(OpenOrderTable);
	
	App *app_ = nullptr;
	OpenOrderModel *model_ = nullptr;
};
}

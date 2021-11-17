#include "OpenOrderTable.hpp"

#include "OpenOrderModel.hpp"

#include <QHeaderView>

namespace cornus::gui {

OpenOrderTable::OpenOrderTable(App *app, OpenOrderModel *model)
: model_(model)
{
	setModel(model);
	setUpdatesEnabled(true);
	resizeColumnsToContents();
	setSelectionMode(QAbstractItemView::SingleSelection);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setAlternatingRowColors(true);
	auto *hh = horizontalHeader();
	hh->setSectionResizeMode(0, QHeaderView::Stretch);
}

OpenOrderTable::~OpenOrderTable() {
	delete model_;
}

QSize
OpenOrderTable::sizeHint() const
{
	int rh = verticalHeader()->defaultSectionSize();
	int h_height = horizontalHeader()->height();
	int total_h = std::min(model_->rowCount() * rh + h_height + 4, 700);
	
	return QSize(300, total_h);
}

} // namespace

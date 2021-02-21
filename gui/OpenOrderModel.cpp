#include "OpenOrderModel.hpp"

#include "OpenOrderPane.hpp"
#include "OpenOrderTable.hpp"
#include "../DesktopFile.hpp"

#include <QIcon>
#include <QScrollBar>

namespace cornus::gui {

OpenOrderModel::OpenOrderModel(App *app, OpenOrderPane *oop):
app_(app), oop_(oop)
{}

OpenOrderModel::~OpenOrderModel() {
	for (auto *next: vec_)
		delete next;
}

void
OpenOrderModel::AppendItem(DesktopFile *item)
{
	int at = vec_.size();
	beginInsertRows(QModelIndex(), at, at);
	vec_.append(item);
	endInsertRows();
}

QVariant
OpenOrderModel::data(const QModelIndex &index, int role) const
{
	if (index.column() != 0) {
		mtl_trace();
		return {};
	}
	
	if (role == Qt::TextAlignmentRole) {
		return Qt::AlignLeft + Qt::AlignVCenter;
	}
	
	const int row = index.row();
	DesktopFile *item = vec_[row];
	
	if (role == Qt::DisplayRole)
	{
		return item->GetName();
	} else if (role == Qt::FontRole) {
//		QStyleOptionViewItem option = table_->option();
//		if (item->selected()) {
//			QFont f = option.font;
//			f.setBold(true);
//			return f;
//		}
	} else if (role == Qt::BackgroundRole) {
//		QStyleOptionViewItem option = table_->option();
//		if (item->is_partition())
//			return option.palette.light();
	} else if (role == Qt::ForegroundRole) {
	} else if (role == Qt::DecorationRole) {
		return item->CreateQIcon();
	}
	return {};
}

QVariant
OpenOrderModel::headerData(int section_i, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		if (orientation == Qt::Horizontal)
		{
			return QString("Applications");
		}
		return {};
	}
	return {};
}

void
OpenOrderModel::RemoveItem(const int index)
{
	beginRemoveRows(QModelIndex(), index, index);
	vec_.remove(index);
	endRemoveRows();
}

void
OpenOrderModel::SetData(QVector<DesktopFile*> &p)
{
	for (auto *next: vec_)
		delete next;
	
	beginInsertRows(QModelIndex(), 0, p.size() - 1);
	vec_ = p;
	endInsertRows();
}

void
OpenOrderModel::SetSelectedRow(const int row_index, const int old_row)
{
	auto *sel_model = table_->selectionModel();
	sel_model->select(createIndex(row_index, 0),
		QItemSelectionModel::Select);
	
	sel_model->select(createIndex(old_row, 0),
		QItemSelectionModel::Deselect);
}

void
OpenOrderModel::UpdateRowRange(int row1, int row2)
{
	int first, last;
	
	if (row1 > row2) {
		first = row2;
		last = row1;
	} else {
		first = row1;
		last = row2;
	}
	const QModelIndex top_left = createIndex(first, 0);
	const QModelIndex bottom_right = createIndex(last, 0);
	emit dataChanged(top_left, bottom_right, {Qt::DisplayRole});
}

void
OpenOrderModel::UpdateVisibleArea() {
	QScrollBar *vs = table_->verticalScrollBar();
	int row_start = table_->rowAt(vs->value());
	int row_count = table_->rowAt(table_->height());
	UpdateRowRange(row_start, row_start + row_count);
}

}



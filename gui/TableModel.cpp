#include "TableModel.hpp"

#include "../App.hpp"
#include "../io/File.hpp"
#include "Table.hpp"

#include <QFont>
#include <QTime>

namespace cornus::gui {

void FullyDelete(io::Files *files) {
	std::lock_guard<std::mutex> guard(files->mutex);
	for (auto *file: files->vec)
		delete file;
	
	files->vec.clear();
	delete files;
}

TableModel::TableModel(cornus::App *app) :
app_(app)//, QAbstractTableModel(parent)
{
	files_ = new io::Files();
}

TableModel::~TableModel()
{
	FullyDelete(files_);
}

QModelIndex
TableModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column);
}

int
TableModel::rowCount(const QModelIndex &parent) const
{
	if (files_ == nullptr)
		return 0;
	
	const std::lock_guard<std::mutex> guard(files_->mutex);
	return files_->vec.size();
}

int
TableModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	
	return i8(Column::Count);
}

QVariant
TableModel::data(const QModelIndex &index, int role) const
{
	const Column col = static_cast<Column>(index.column());
	
	if (role == Qt::TextAlignmentRole) {
		if (col == Column::FileName)
			return Qt::AlignLeft + Qt::AlignVCenter;
		if (col == Column::Icon)
			return Qt::AlignHCenter + Qt::AlignVCenter;
		
		return Qt::AlignLeft /*Qt::AlignHCenter*/ + Qt::AlignVCenter;
	}
	
	std::lock_guard<std::mutex> guard(files_->mutex);
	
	const int row = index.row();
	
	if (row >= files_->vec.size())
		return {};
	
	io::File *file = files_->vec[row];
	
	if (role == Qt::DisplayRole)
	{
		if (col == Column::FileName) {
			//return ColumnFileNameData(file);
		} else if (col == Column::Size) {
			if (file->is_dir_or_so())
				return QString();
			
			const i64 sz = file->size();
			float rounded;
			QString type;
			if (sz >= io::TiB) {
				rounded = sz / io::TiB;
				type = QLatin1String(" TiB");
			}
			else if (sz >= io::GiB) {
				rounded = sz / io::GiB;
				type = QLatin1String(" GiB");
			} else if (sz >= io::MiB) {
				rounded = sz / io::MiB;
				type = QLatin1String(" MiB");
			} else if (sz >= io::KiB) {
				rounded = sz / io::KiB;
				type = QLatin1String(" KiB");
			} else {
				rounded = sz;
				type = QLatin1String(" bytes");
			}
			
			return io::FloatToString(rounded, 1) + type;
		} else if (col == Column::Icon) {
			if (file->is_regular())
				return {};
			if (file->is_symlink())
				return QLatin1String("L");
			if (file->is_bloc_device())
				return QLatin1String("B");
			if (file->is_char_device())
				return QLatin1String("C");
			if (file->is_socket())
				return QLatin1String("S");
			if (file->is_pipe())
				return QLatin1String("P");
		}
	} else if (role == Qt::FontRole) {
		QFont font;
//		if (col == Column::Icon)
//			font.setBold(true);
		return font;
	} else if (role == Qt::BackgroundRole) {
		if (row % 2)
			return {};
		return QColor(245, 245, 245);
	} else if (role == Qt::ForegroundRole) {
		if (col == Column::Icon)
			return QColor(0, 0, 155);
		return {};
	} else if (role == Qt::DecorationRole) {
		if (col != Column::Icon)
			return {};
		
		if (file->cache().icon != nullptr)
			return *file->cache().icon;
		
		app_->LoadIcon(*file);
		
		if (file->cache().icon == nullptr)
			return {};
		
		return *file->cache().icon;
	}
	
	return {};
}

QVariant
TableModel::headerData(int section_i, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		if (orientation == Qt::Horizontal)
		{
			const Column section = static_cast<Column>(section_i);
			
			switch (section) {
			case Column::Icon:
				return QString();
			case Column::FileName:
				return QLatin1String("Name");
			case Column::Size:
				return QLatin1String("Size");
			default: {
				mtl_trace();
				return {};
			}
			}
		}
		return QString::number(section_i + 1);
	}
	return {};
}

bool
TableModel::InsertRows(const i32 at, const QVector<cornus::io::File*> &files_to_add)
{
	std::lock_guard<std::mutex> guard(files_->mutex);
	
	if (files_->vec.isEmpty())
		return false;
	
	const int first = at;
	const int last = at + files_to_add.size() - 1;
	
	beginInsertRows(QModelIndex(), first, last);
	
	for (i32 i = 0; i < files_to_add.size(); i++)
	{
		auto *song = files_to_add[i];
		files_->vec.insert(at + i, song);
	}
	
	endInsertRows();
	
	return true;
}

bool
TableModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (count <= 0)
		return false;
	
	CHECK_TRUE((count == 1));
	
	std::lock_guard<std::mutex> guard(files_->mutex);
	
	const int first = row;
	const int last = row + count - 1;
	beginRemoveRows(QModelIndex(), first, last);
	
	auto &vec = files_->vec;
	
	for (int i = count - 1; i >= 0; i--) {
		const i32 index = first + i;
		auto *item = vec[index];
		vec.erase(vec.begin() + index);
		delete item;
	}
	
	endRemoveRows();
	return true;
}

void
TableModel::SwitchTo(io::Files *files)
{
	int prev_count = 0;
	{
		std::lock_guard<std::mutex> guard(files_->mutex);
		prev_count = files_->vec.size();
	}
	
	beginRemoveRows(QModelIndex(), 0, prev_count);
	FullyDelete(files_);
	endRemoveRows();
	
	int count = files->vec.size();
	files_ = files;
	beginInsertRows(QModelIndex(), 0, count);
	endInsertRows();
}

void
TableModel::UpdateRange(int row1, Column c1, int row2, Column c2)
{
	int first, last;
	
	if (row1 > row2) {
		first = row2;
		last = row1;
	} else {
		first = row1;
		last = row2;
	}
	
	const QModelIndex top_left = createIndex(first, int(c1));
	const QModelIndex bottom_right = createIndex(last, int(c2));
	emit dataChanged(top_left, bottom_right, {Qt::DisplayRole});
}

}

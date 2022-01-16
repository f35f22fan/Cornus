#include "CompleterModel.hpp"

#include "Location.hpp"
#include "../io/io.hh"

namespace cornus::gui {

CompleterModel::CompleterModel(Location *location):
location_(location)
{
	Q_UNUSED(location_);
}

CompleterModel::~CompleterModel()
{}

QVariant
CompleterModel::data(const QModelIndex &index, int role) const
{
	if (index.column() != 0)
		return {};
	
	if (role == Qt::TextAlignmentRole)
		return Qt::AlignLeft + Qt::AlignVCenter;
	
	const int row = index.row();
	
	if (role == Qt::DisplayRole || role == Qt::EditRole)
	{
		QString s = items_.dir_path;
		if (s != QLatin1String("/"))
			s.append('/');
		return s + items_.vec[row];
	} else if (role == Qt::DecorationRole) {
		return QIcon::fromTheme(QLatin1String("folder"));
	}
	return {};
}

int CompleterModel::rowCount(const QModelIndex &parent) const
{
	return items_.vec.size();
}

void CompleterModel::SetRootPath(QString root_path)
{
	int index = root_path.lastIndexOf('/');
	if (index == -1)
		return;
	QString dir = root_path.mid(0, index);
	if (dir.isEmpty())
		dir.append('/');
	
	if (items_.dir_path == dir)
		return;
		
	items_.dir_path = dir;
	beginRemoveRows(QModelIndex(), 0, items_.vec.size());
		items_.vec.clear();
	endRemoveRows();
	
	QVector<QString> v;
	io::ListDirNames(items_.dir_path, v);
	
	beginInsertRows(QModelIndex(), 0, v.size());
		items_.vec = v;
	endInsertRows();
	
}

}



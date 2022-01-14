#include "TableHeader.hpp"

#include "../App.hpp"
#include "../io/File.hpp"
#include "../io/io.hh"
#include "Tab.hpp"
#include "Table.hpp"

#include <QDragEnterEvent>
#include <QDir>

namespace cornus::gui {

TableHeader::TableHeader(Table *parent):
QHeaderView(Qt::Horizontal, parent), table_(parent)
{
	setSortIndicatorShown(true);
	setSectionsMovable(false);
//	setMouseTracking(true);
	setSectionsClickable(true);
	{
		setDragEnabled(true);
		setAcceptDrops(true);
//		setDragDropOverwriteMode(false);
//		setDropIndicatorShown(true);
//		setDefaultDropAction(Qt::MoveAction);
	}
}

TableHeader::~TableHeader() {}

void TableHeader::dragEnterEvent(QDragEnterEvent *evt)
{
	in_drag_mode_ = true;
	evt->acceptProposedAction();
	RepaintName();
}

void TableHeader::dragLeaveEvent(QDragLeaveEvent *evt)
{
	in_drag_mode_ = false;
	RepaintName();
}

void TableHeader::dragMoveEvent(QDragMoveEvent *evt)
{
	table_->AutoScroll(VDirection::Up);
}

void TableHeader::dropEvent(QDropEvent *evt)
{
	in_drag_mode_ = false;
	RepaintName();
	App *app = table_->app();
	if (evt->mimeData()->hasUrls())
	{
		QVector<io::File*> *files_vec = new QVector<io::File*>();
		
		for (const QUrl &url: evt->mimeData()->urls())
		{
			io::File *file = io::FileFromPath(url.path());
			if (file != nullptr)
				files_vec->append(file);
		}
		
		const QString current_dir = app->tab()->current_dir();
		QDir up_dir(current_dir);
		QString to_dir_path;
		if (up_dir.cdUp()) {
			to_dir_path = up_dir.absolutePath();
		}
		
		io::File *to_dir = nullptr;
		if (to_dir_path != current_dir) {
			to_dir = io::FileFromPath(to_dir_path);
		}
		
		if (to_dir != nullptr) {
			table_->tab()->ExecuteDrop(files_vec, to_dir, evt->proposedAction(), evt->possibleActions());
		} else {
			for (auto *next: *files_vec)
				delete next;
			delete files_vec;
		}
	}
}

void TableHeader::mousePressEvent(QMouseEvent *evt)
{
	QHeaderView::mousePressEvent(evt);
//	const auto &pos = evt->pos();
//	const int col = logicalIndexAt(pos);//index.column();
//mtl_info("Col: %d, x,y: %d,%d", col, pos.x(), pos.y());
//static auto order = Qt::DescendingOrder;
//	order = (order == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
//	Q_EMIT sortIndicatorChanged(col, order);
}

void TableHeader::mouseReleaseEvent(QMouseEvent *evt)
{
	QHeaderView::mouseReleaseEvent(evt);
}

void TableHeader::RepaintName()
{
	const int col = (int)Column::FileName;
	headerDataChanged(Qt::Horizontal, col, col);
}

}



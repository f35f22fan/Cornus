#pragma once

#include "decl.hxx"
#include "../decl.hxx"
#include "../io/decl.hxx"

#include <QAbstractItemModel>

Q_DECLARE_METATYPE(cornus::gui::InsertArgs);

namespace cornus::gui {
class TreeModel : public QAbstractItemModel
{
	Q_OBJECT
	
public:
	explicit TreeModel(cornus::App *app, QObject *parent = nullptr);
	~TreeModel();
	
	QVariant data(const QModelIndex &index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QVariant headerData(int section, Qt::Orientation orientation,
	int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column,
	const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	int rowCount(const QModelIndex &parent) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	// returns num bookmarks added or -1 on error
	int AddBookmarks(QVector<io::File *> &files_vec, const QPoint &pos);
	App* app() const { return app_; }
	void DeleteSelectedBookmark();
	void MountEvent(const QString &path, const QString &fs_uuid, const PartitionEventType evt);
	void MoveBookmarks(QStringList str_list, const QPoint &pos);
	void SetView(TreeView *p);
	void UpdateIndices(const QModelIndex &top_left, const QModelIndex &bottom_right);
	void UpdateIndex(const QModelIndex &index);
	void UpdateVisibleArea();
	
public Q_SLOTS:
	void DeviceEvent(const cornus::Device device,
		const cornus::DeviceAction action,
		const QString dev_path, const QString sys_path);
	
	void InsertFromAnotherThread(cornus::gui::InsertArgs args);
	
private:
	
	int GetDropLocation(const QPoint &pos, QModelIndex &target);
	gui::TreeItem* GetRootTS(const io::DiskInfo &disk_info, int *row);
	bool InsertBookmarkAt(const i32 at, TreeItem *item);
	void InsertBookmarks(const QVector<cornus::gui::TreeItem*> &bookmarks);
	bool InsertPartition(TreeItem *item);
	void InsertPartitions(const QVector<cornus::gui::TreeItem*> &partitions);
	void SetupModelData();
	
	cornus::App *app_ = nullptr;
	TreeView *view_ = nullptr;
};
}

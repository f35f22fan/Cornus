#include "TreeData.hpp"

#include "gui/TreeItem.hpp"

Q_DECLARE_METATYPE(QVector<cornus::gui::TreeItem *>);

namespace cornus {

TreeData::TreeData() {}

TreeData::~TreeData()
{
	// .roots deleted by hand in App destructor.
}

void TreeData::BroadcastPartitionsLoadedLTH()
{
	MutexGuard g = guard();
	partitions_loaded = true;
	int status = pthread_cond_broadcast(&cond);
	if (status != 0)
		mtl_status(status);
}

gui::TreeItem* TreeData::GetBookmarksRootNTS(int *index) const
{
	int i = -1;
	for (gui::TreeItem *next: roots)
	{
		i++;
		if (next->is_bookmarks_root()) {
			if (index)
				*index = i;
			return next;
		}
	}
	
	return nullptr;
}

gui::TreeItem* TreeData::GetCurrentPartitionNTS() const
{
	for (gui::TreeItem *root: roots)
	{
		if (!root->is_disk())
			continue;
		for (gui::TreeItem *child: root->children()) {
			if (child->current_partition())
				return child;
		}
	}
	
	return nullptr;
}

gui::TreeItem* TreeData::GetPartitionByMountPathNTS(const QString &full_path)
{
	int largest = 0;
	gui::TreeItem *current = nullptr;
	
	for (gui::TreeItem *root: roots)
	{
		if (!root->is_disk())
			continue;
		for (gui::TreeItem *child: root->children()) {
			if (child->mounted() && full_path.startsWith(child->mount_path())) {
				if (largest < child->mount_path().size()) {
					current = child;
					largest = child->mount_path().size();
					//mtl_printq2("Found: ", child->mount_path());
				}
			}
		}
	}
	
	return current;
}

void TreeData::MarkCurrentPartition(const QString &full_path)
{
	auto g = guard();
	gui::TreeItem *current = GetPartitionByMountPathNTS(full_path);
	if (current != nullptr)
		current->current_partition(true);
	
	for (gui::TreeItem *root: roots)
	{
		if (!root->is_disk())
			continue;
		for (gui::TreeItem *child: root->children()) {
			if (child != current && child->current_partition()) {
				child->current_partition(false);
			}
		}
	}
}

MutexGuard TreeData::try_guard()
{
	return MutexGuard(&mutex, LockType::TryLock);
}

}

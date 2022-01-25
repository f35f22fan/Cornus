#include "Files.hpp"

#include "File.hpp"

#include <sys/eventfd.h>
#include <QMetaType> /// Q_DECLARE_METATYPE()

namespace cornus::io {

FilesData::FilesData()
{
	signal_quit_fd = ::eventfd(0, 0);
	if (signal_quit_fd == -1)
		mtl_status(errno);
}

FilesData::~FilesData()
{
	const int status = ::close(signal_quit_fd);
	if (status != 0)
		mtl_status(errno);
	
	for (auto *next: vec) {
		delete next;
	}
	vec.clear();
}

io::File* Files::GetFileAtIndex_Lock(const int index)
{
	MutexGuard guard = this->guard();
	auto &vec = data.vec;
	if (index < 0 | index >= vec.size())
		return nullptr;
	
	return vec[index]->Clone();
}

int Files::GetFirstSelectedFile_Lock(io::File **ret_cloned_file)
{
	MutexGuard guard = this->guard();
	
	int i = 0;
	for (io::File *file: data.vec) {
		if (file->is_selected()) {
			if (ret_cloned_file != nullptr)
				*ret_cloned_file = file->Clone();
			return i;
		}
		i++;
	}
	
	return -1;
}

QString Files::GetFirstSelectedFileFullPath_Lock(QString *ext)
{
	MutexGuard guard = this->guard();
	
	for (io::File *file: data.vec) {
		if (file->is_selected()) {
			if (ext != nullptr)
				*ext = file->cache().ext.toString().toLower();
			return file->build_full_path();
		}
	}
	
	return QString();
}

int Files::GetSelectedFilesCount_Lock(QVector<QString> *extensions)
{
	MutexGuard guard = this->guard();
	int selected_count = 0;
	
	for (io::File *next: data.vec)
	{
		if (next->is_selected())
		{
			selected_count++;
			if ((extensions != nullptr) && next->is_regular())
			{
				extensions->append(next->cache().ext.toString());
			}
		}
	}
	
	return selected_count;
}

void Files::GetSelectedFileNames(QVector<QString> &names,
	const Path path,
	const StringCase str_case)
{
	MutexGuard guard = this->guard();
	const bool OnlyName = (path == Path::OnlyName);
	for (io::File *next: data.vec)
	{
		if (next->is_selected()) {
			switch (str_case) {
			case StringCase::AsIs: {
				names.append(OnlyName ? next->name() : next->build_full_path());
				break;
			}
			case StringCase::Lower: {
				names.append(OnlyName ? next->name_lower()
					: next->build_full_path().toLower());
				break;
			}
			default: {
				mtl_trace();
			}
			} /// switch()
		}
	}
	
}

QPair<int, int> Files::ListSelectedFiles_Lock(QList<QUrl> &list)
{
	MutexGuard guard = this->guard();
	int num_files = 0;
	int num_dirs = 0;
	
	for (io::File *next: data.vec)
	{
		if (next->is_selected())
		{
			if (next->is_dir())
				num_dirs++;
			else
				num_files++;
			const QString s = next->build_full_path();
			list.append(QUrl::fromLocalFile(s));
		}
	}
	
	return QPair(num_dirs, num_files);
}

void Files::RemoveThumbnailsFromSelectedFiles()
{
	MutexGuard guard = this->guard();
	
	for (io::File *next: data.vec)
	{
		if (!next->is_selected() || !next->has_thumbnail_attr())
			continue;
		
		io::RemoveXAttr(next->build_full_path(), media::XAttrThumbnail);
	}
}

void Files::SelectAllFiles_NoLock(const Selected flag, QSet<int> &indices)
{
	int i = -1;
	for (io::File *file: data.vec)
	{
		i++;
		if (file->selected() != flag)
		{
			indices.insert(i);
			file->selected(flag);
		}
	}
}

void Files::SelectFilenamesLater(const QVector<QString> &names, const SameDir sd)
{
	MutexGuard guard = this->guard();
	auto dir_to_skip = (sd == SameDir::Yes) ? -1 : data.dir_id;
	data.skip_dir_id = dir_to_skip;
	for (const auto &name: names)
	{
		data.filenames_to_select.insert(name, 0);
	}
}

void Files::SelectFileRange_NoLock(const int row1, const int row2, QSet<int> &indices)
{
	QVector<io::File*> &vec = data.vec;
	
	if (row1 < 0 || row1 >= vec.size() || row2 < 0 || row2 >= vec.size()) {
///		mtl_warn("row1: %d, row2: %d", row1, row2);
		return;
	}
	
	int row_start = row1;
	int row_end = row2;
	if (row_start > row_end)
	{
		row_start = row2;
		row_end = row1;
	}
	
	for (int i = row_start; i <= row_end; i++)
	{
		vec[i]->set_selected(true);
		indices.insert(i);
	}
}

} // cornus::io:

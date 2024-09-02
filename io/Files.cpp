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
	if (::close(signal_quit_fd) != 0)
		mtl_status(errno);
	
	for (auto *next: vec) {
		delete next;
	}
	vec.clear();
}

io::File* Files::GetFileAtIndex(const cornus::Lock l, cint index)
{
	MutexGuard guard = this->guard(l);
	auto &vec = data.vec;
	if (index < 0 | index >= vec.size())
		return nullptr;
	
	return vec[index]->Clone();
}

int Files::GetFirstSelectedFile(const cornus::Lock l, io::File **ret_cloned_file,
	const Clone c)
{
	auto g = this->guard(l);
	int i = 0;
	for (io::File *file: data.vec)
	{
		if (file->is_selected())
		{
			if (ret_cloned_file != nullptr)
				*ret_cloned_file = (c == Clone::Yes) ? file->Clone() : file;
			return i;
		}
		i++;
	}
	
	return -1;
}

QString Files::GetFirstSelectedFileFullPath(const cornus::Lock l, QString *ext)
{
	MutexGuard guard = this->guard(l);
	
	for (io::File *file: data.vec) {
		if (file->is_selected()) {
			if (ext != nullptr)
				*ext = file->cache().ext.toLower();
			return file->build_full_path();
		}
	}
	
	return QString();
}

QList<io::File*> Files::GetSelectedFiles(const cornus::Lock l, const cornus::Clone clone)
{
	auto g = this->guard(l);
	QList<io::File*> ret;
	for (io::File *file: data.vec)
	{
		if (file->is_selected())
		{
			ret.append((clone == Clone::Yes) ? file->Clone() : file);
		}
	}
	
	return ret;
}

int Files::GetSelectedFilesCount(const cornus::Lock l, QVector<QString> *extensions)
{
	MutexGuard guard = this->guard(l);
	int selected_count = 0;
	
	for (io::File *next: data.vec)
	{
		if (next->is_selected())
		{
			selected_count++;
			if ((extensions != nullptr) && next->is_regular())
			{
				extensions->append(next->cache().ext);
			}
		}
	}
	
	return selected_count;
}

void Files::GetSelectedFileNames(const cornus::Lock l, QVector<QString> &names,
	const Path path, const StringCase str_case)
{
	MutexGuard guard = this->guard(l);
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

bool Files::HasSelectedFiles(const cornus::Lock l) const
{
	MutexGuard guard = this->guard(l);
	
	for (io::File *next: data.vec)
	{
		if (next->is_selected())
			return true;
	}
	
	return false;
}

QPair<int, int> Files::ListSelectedFiles(const cornus::Lock l, QList<QUrl> &list)
{
	MutexGuard guard = this->guard(l);
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
	
	return QPair<int, int> (num_dirs, num_files);
}


void Files::MarkFilesAsWatched(const enum Lock l, QList<io::File*> &vec)
{
	if (vec.isEmpty())
		return;
	
	auto g = guard(l);
	cauto &key = media::WatchProps::Name;
	
	for (io::File *file: vec) {
		for (io::File *next: data.vec)
		{
			if (next->id() != file->id())
				continue;
			
			QString full_path = next->build_full_path();
			if (next->has_watched_attr())
			{
				io::RemoveEFA(full_path, {key});
			} else {
				ByteArray value;
				value.add_u64(media::WatchProps::Watched);
				io::SetEFA(full_path, key, value);
			}
			break;
		}
	}
}

void Files::RemoveThumbnailsFromSelectedFiles()
{
	MutexGuard guard = this->guard();
	
	for (io::File *next: data.vec)
	{
		if (!next->is_selected() || !next->has_thumbnail_attr())
			continue;
		
		const auto path = next->build_full_path();
		io::RemoveEFA(path, {media::XAttrName, media::XAttrThumbnail});
	}
}

void Files::SelectAllFiles(const cornus::Lock l, const Selected flag, QSet<int> &indices)
{
	auto g = this->guard(l);
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
	auto g = this->guard();
	data.skip_dir_id = (sd == SameDir::Yes) ? -1 : data.dir_id;
	for (const auto &name: names)
	{
		data.filenames_to_select.insert(name, 0);
	}
}

void Files::SelectFileRange(const cornus::Lock l, cint row1, cint row2, QSet<int> &indices)
{
	auto guard = this->guard(l);
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

void Files::SetLastWatched(const enum Lock l, io::File *needle)
{
	MTL_CHECK_VOID(needle);
	auto g = guard(l);
	for (io::File *next: data.vec)
	{
		if (needle->id() == next->id())
		{
			next->WatchProp(Op::Invert, media::WatchProps::LastWatched);
			break;
		}
	}
}

void Files::WakeUpInotify(const enum Lock l)
{
	auto g = guard(l);
	// Wake up epoll(), must write an 8 bytes number.
	ci64 n = 1;
	if (::write(data.signal_quit_fd, &n, sizeof n) == -1)
		mtl_status(errno);
}

} // cornus::io:

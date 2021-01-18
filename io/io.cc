#include "io.hh"

#include "File.hpp"
#include "../err.hpp"
#include "../ByteArray.hpp"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <bits/stdc++.h> /// std::sort()
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace cornus::io {

void Delete(io::File *file) {
	if (!file->is_dir()) {
		file->DeleteFromDisk();
		return;
	}
	
	io::Files files;
	files.dir_path = file->build_full_path();
	files.show_hidden_files = true;
	CHECK_TRUE_VOID((ListFiles(files) == io::Err::Ok));
	
	for (io::File *next: files.vec) {
		if (next->is_dir()) {
			Delete(next);
		}
		next->DeleteFromDisk();
		delete next;
	}
	
	file->DeleteFromDisk();
}

bool
EnsureDir(const QString &dir_path, const QString &subdir)
{
	auto d = dir_path;
	
	if (!d.endsWith('/'))
		d.append('/');
	
	d.append(subdir);
	
	auto ba = d.toLocal8Bit();
	FileType ft;
	
	if (FileExistsCstr(ba.data(), &ft))
	{
		if (ft != FileType::Dir)
		{
			if (remove(ba.data()) == 0)
				return mkdir(ba.data(), DirPermissions) == 0;
			return false;
		}
		
		return true;
	}
	
	return mkdir(ba.data(), DirPermissions) == 0;
}

bool
FileExistsCstr(const char *path, FileType *file_type)
{
	struct stat st;
	
	if (lstat(path, &st) != 0)
		return false;
	
	if (file_type != nullptr)
		*file_type = MapPosixTypeToLocal(st.st_mode);
	
	return true;
}

const char*
FileTypeToString(const FileType t)
{
	if (t == FileType::Regular)
		return "Regular";
	if (t == FileType::Dir)
		return "Folder";
	if (t == FileType::BlockDevice)
		return "Block device";
	if (t == FileType::CharDevice)
		return "Char device";
	if (t == FileType::Pipe)
		return "Pipe";
	if (t == FileType::Socket)
		return "Socket";
	if (t == FileType::Symlink)
		return "Symlink";

	return nullptr;
}

void
FillInStx(io::File &file, const struct statx &stx, const QString *name)
{
	using io::FileType;
	if (name != nullptr)
		file.name(*name);
	file.mode(stx.stx_mode);
	file.size(stx.stx_size);
	file.type(MapPosixTypeToLocal(stx.stx_mode));
	file.id(io::FileID::NewStx(stx));
	file.time_created(stx.stx_btime);
	file.time_modified(stx.stx_mtime);
}

QString
FloatToString(const float number, const int precision)
{
	float rem = i32(number) - number;

	if (IsNearlyEqual(rem, 0.0))
		return QString::number(i32(number));
	
	return QString::number(number, 'f', precision);
}

QStringRef
GetFilenameExtension(const QString &name)
{
	int dot = name.lastIndexOf('.');
	
	if (dot == -1 || (dot == name.size() - 1))
		return QStringRef();
	
	return name.midRef(dot + 1);
}

bool IsNearlyEqual(double x, double y)
{
	const double epsilon = 1e-5;
	return std::abs(x - y) <= epsilon * std::abs(x);
	// see Knuth section 4.2.2 pages 217-218
}

io::Err
ListFileNames(const QString &full_dir_path, QVector<QString> &vec)
{
	struct dirent *entry;
	auto dir_path_ba = full_dir_path.toLocal8Bit();
	DIR *dp = opendir(dir_path_ba.data());
	
	if (dp == NULL)
		return MapPosixError(errno);
	
	QString dir_path = full_dir_path;
	
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	while ((entry = readdir(dp)))
	{
		QString name(entry->d_name);
		
		if (name.startsWith(QChar('.')))
		{
			if (name == QLatin1String(".") || name == QLatin1String(".."))
				continue;
		}
		
		vec.append(name);
	}
	
	closedir(dp);
	
	return Err::Ok;
}

io::Err
ListFiles(io::Files &files, FilterFunc ff)
{
	if (!files.dir_path.endsWith('/'))
		files.dir_path.append('/');
	
	auto dir_path_ba = files.dir_path.toLocal8Bit();
	DIR *dp = opendir(dir_path_ba.data());
	
	if (dp == NULL) {
		mtl_printq2("Can't list dir: ", files.dir_path);
		return MapPosixError(errno);
	}
	
	const bool hide_hidden_files = !files.show_hidden_files;
	struct dirent *entry;
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_ALL;
	
	while ((entry = readdir(dp)))
	{
		const char *n = entry->d_name;
		if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
			continue;
		
		QString name(n);
		
		if (hide_hidden_files && name.startsWith('.'))
			continue;
		
		if (ff != nullptr && !ff(files.dir_path, name))
			continue;
		
		QString full_path = files.dir_path + name;
		auto ba = full_path.toLocal8Bit();
		
		if (statx(0, ba.data(), flags, fields, &stx) != 0) {
			mtl_warn("statx(): %s", strerror(errno));
			continue;
		}
		
		auto *file = new io::File(&files);
		FillInStx(*file, stx, &name);
		
		if (file->is_symlink()) {
			auto *target = new LinkTarget();
			ReadLink(ba.data(), *target, files.dir_path);
			file->link_target_ = target;
		}
		
		files.vec.append(file);
	}
	
	closedir(dp);
	std::sort(files.vec.begin(), files.vec.end(), cornus::io::SortFiles);
	
	return Err::Ok;
}

Err
MapPosixError(int e)
{
	using io::Err;
	
	switch (e) {
	case EACCES: return Err::Access;
	case EIO: return Err::IO;
	default: return Err::Other;
	}
}

void ReadLink(const char *file_path, LinkTarget &link_target,
	const QString &parent_dir) {
	
	if (link_target.cycles == LinkTarget::MaxCycles) {
		link_target.cycles *= -1;
		mtl_warn("Symlink chain too large");
		return;
	}
	
	link_target.chain_paths_.append(file_path);
	
	struct stat st;
	
	if (lstat(file_path, &st) == -1) {
		mtl_trace("lstat: %s, file: \"%s\"", strerror(errno), file_path);
		return;
	}
	
	FileID file_id = FileID::New(st);
	if (link_target.chain_ids_.contains(file_id)) {
		link_target.cycles *= -1;
		return;
	} else {
		link_target.chain_ids_.append(file_id);
	}
	
	/* Add one to the link size, so that we can determine whether
	the buffer returned by readlink() was truncated. */
	i64 bufsize = st.st_size + 1;
	
	/* Some magic symlinks under (for example) /proc and /sys
	report 'st_size' as zero. In that case, take PATH_MAX as
	a "good enough" estimate. */
	if (st.st_size == 0)
		bufsize = PATH_MAX;
	
	char *buf = (char*)malloc(bufsize);
	
	if (buf == NULL) {
		mtl_trace("malloc: %s", strerror(errno));
		return;
	}
	
	auto nbytes = readlink(file_path, buf, bufsize);
	
	if (nbytes == -1) {
		free(buf);
		mtl_trace("readlink: %s, file_path: \"%s\"", strerror(errno), file_path);
		return;
	}
	
	/* If the return value was equal to the buffer size, then the
	the link target was larger than expected (perhaps because the
	target was changed between the call to lstat() and the call to
	readlink()). Warn the user that the returned target may have
	been truncated. */
	if (nbytes == bufsize) {
		mtl_warn("Returned buffer may have been truncated");
		return;
	}
	
	buf[nbytes] = 0;
	QString full_target_path = QString::fromUtf8(buf);
	if (!full_target_path.startsWith('/')) {
		QString s = parent_dir;
		if (!parent_dir.endsWith('/'))
			s.append('/');
		s.append(full_target_path);
		full_target_path = s;
	}
	free(buf);
	buf = NULL;
	full_target_path = QDir::cleanPath(full_target_path);
	auto ba = full_target_path.toLocal8Bit();
	
	if (lstat(ba.data(), &st) != -1) {
		if ((st.st_mode & S_IFMT) == S_IFLNK) {
			QDir dir(full_target_path);
			if (!dir.cdUp()) {
				mtl_trace();
				return;
			}
			QString parent_dir2 = dir.absolutePath();
			link_target.cycles++;
			ReadLink(ba.data(), link_target, parent_dir2);
			return;
		}
	} else {
		//mtl_trace("lstat: %s, file: \"%s\"", strerror(errno), ba.data());
		return;
	}
	
	link_target.chain_paths_.append(full_target_path);
	link_target.path = full_target_path;
	link_target.mode = st.st_mode;
	link_target.type = MapPosixTypeToLocal(st.st_mode);
}

isize
TryReadFile(const QString &full_path, char *buf, const i64 how_much,
	ExecInfo *info)
{
	auto ba = full_path.toLocal8Bit();
	struct statx stx;
	const auto fields = STATX_MODE;
	
	if (statx(0, ba.data(), AT_SYMLINK_NOFOLLOW, fields, &stx) != 0) {
		//mtl_warn("statx(): %s", strerror(errno));
		return -1;
	}
	
	if (info != nullptr)
		info->mode = stx.stx_mode;
	
	const int fd = open(ba.data(), O_RDONLY);
	if (fd == -1)
		return -1;
	
	isize ret = read(fd, buf, how_much);
	close(fd);
	return ret;
}

io::Err
ReadFile(const QString &full_path, cornus::ByteArray &buffer,
	const i64 read_max)
{
	// This function follows links, it makes no sense not to
	// because I already show the link target in the table view.
	struct stat st;
	auto path = full_path.toLocal8Bit();
	
	if (stat(path.data(), &st) != 0)
		return MapPosixError(errno);
	
	i64 MAX = (read_max == -1) ? st.st_size :
		std::min(read_max, st.st_size);
	
	if (MAX <= 0) {
		MAX = 1024 * 10;
	}
	
	buffer.alloc(MAX);
	char *buf = buffer.data();
	const int fd = open(path.data(), O_RDONLY);
	
	if (fd == -1)
		return MapPosixError(errno);
	
	isize so_far = 0;
	
	while (so_far < MAX) {
		isize ret = read(fd, buf + so_far, MAX - so_far);
		mtl_info("Read: %ld", ret);
		if (ret == -1) {
			if (errno == EAGAIN)
				continue;
			io::Err e = MapPosixError(errno);
			mtl_warn("ReadFile: %s", strerror(errno));
			close(fd);
			return e;
		} else if (ret == 0) {
			// zero indicates the end of file
			mtl_info("GOT zero!!!!");
			break;
		}
		
		so_far += ret;
	}
	
	close(fd);
	buffer.size(so_far);
	
	return Err::Ok;
}

bool ReloadMeta(io::File &file)
{
	auto ba = file.build_full_path().toLocal8Bit();
	
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_ALL;
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0) {
		mtl_warn("statx(): %s", strerror(errno));
		return false;
	}
	
	FillInStx(file, stx, nullptr);
	
	return true;
}

io::Err
SameFiles(const QString &path1, const QString &path2, bool &same)
{
	struct stat st;
	auto ba = path1.toLocal8Bit();
	
	if (lstat(ba.data(), &st) == -1)
		return MapPosixError(errno);
	
	auto id1 = FileID::New(st);
	ba = path2.toLocal8Bit();
	
	if (lstat(ba.data(), &st) == -1)
		return MapPosixError(errno);
	
	auto id2 = FileID::New(st);
	same = id1 == id2;
	
	return io::Err::Ok;
}

bool
SortFiles(io::File *a, io::File *b) 
{
/** Note: this function MUST be implemented with strict weak ordering
  otherwise it randomly crashes (because of undefined behavior),
  more info here:
 https://stackoverflow.com/questions/979759/operator-and-strict-weak-ordering/981299#981299 */
	const SortingOrder order = a->files()->sorting_order;
	
	if (a->is_dir_or_so() && !b->is_dir_or_so())
		return true;
	else if (b->is_dir_or_so() && !a->is_dir_or_so())
		return false;
	
	if (order.column == gui::Column::FileName) {
		if (a->name_lower() == b->name_lower())
			return false;
		
		bool result = a->name_lower() < b->name_lower();
		return order.ascending ? result : !result;
	}
	
	const bool by_created = order.column == gui::Column::TimeCreated;
	if (by_created || order.column == gui::Column::TimeModified)
	{
		auto &tc = by_created ? a->time_created() : a->time_modified();
		auto &tc2 = by_created ? b->time_created() : a->time_modified();
		
		if (tc.tv_sec != tc2.tv_sec) {
			const bool less_sec = (tc.tv_sec < tc2.tv_sec);
			return order.ascending ? less_sec : !less_sec;
		}
		
		if (tc.tv_nsec == tc2.tv_nsec)
			return false;
		
		const bool less_nsec = tc.tv_nsec < tc2.tv_nsec;
		return order.ascending ? less_nsec : !less_nsec;
	}
	
	if (order.column == gui::Column::Size) {
		if (a->size() == b->size()) {
			if (a->name_lower() == b->name_lower())
				return false;
			
			bool result = a->name_lower() < b->name_lower();
			return order.ascending ? result : !result;
		}
		const bool less_size = a->size() < b->size();
		return order.ascending ? less_size : !less_size;
	}
	
	if (order.column == gui::Column::Icon) {
		// order by file type..
		if (a->type() != b->type()) {
			const bool less_type = a->type() < b->type();
			return order.ascending ? less_type : !less_type;
		}
		// next, order by extension:
		if (a->cache().ext == b->cache().ext) {
			if (a->name_lower() == b->name_lower())
				return false;
			
			bool result = a->name_lower() < b->name_lower();
			return order.ascending ? result : !result;
		}
		
		const bool less_ext = a->cache().ext < b->cache().ext;
		return order.ascending ? less_ext : !less_ext;
	}
	
	mtl_trace();
	return false;
}

io::Err
WriteToFile(const QString &full_path, const char *data, const i64 size)
{
	auto path = full_path.toLocal8Bit();
	const int fd = open(path.data(), O_WRONLY | O_CREAT | O_TRUNC,
		io::FilePermissions);
	
	if (fd == -1)
		return MapPosixError(errno);
	
	isize written = 0;
	
	while (written < size) {
		// ssize_t write(int fd, const void *buf, size_t count);
		isize ret = write(fd, data + written, size - written);
		
		if (ret == -1) {
			if (errno == EAGAIN)
				continue;
			io::Err e = MapPosixError(errno);
			close(fd);
			return e;
		}
		
		written += ret;
	}
	
	close(fd);
	
	return Err::Ok;
}

} // cornus::io::



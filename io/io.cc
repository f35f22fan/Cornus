#include "io.hh"

#include "../AutoDelete.hh"
#include "../ExecInfo.hpp"
#include "../str.hxx"
#include "File.hpp"
#include "../err.hpp"
#include "../ByteArray.hpp"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <bits/stdc++.h> /// std::sort()
#include <sys/stat.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace cornus::io {

void Notify::Init()
{
	if (fd == -1)
	{
		fd = inotify_init();
		if (fd == -1)
			mtl_status(errno);
	}
}

void Notify::Close()
{
	if (fd != -1 && pthread_mutex_lock(&watches_mutex) == 0) {
		close(fd);
		fd = -1;
		pthread_mutex_unlock(&watches_mutex);
	}
}

bool CopyFileFromTo(const QString &from_full_path, QString to_dir)
{
	if (!to_dir.endsWith('/'))
		to_dir.append('/');
	
	QStringRef name = io::GetFileNameOfFullPath(from_full_path);
	
	if (name.isEmpty())
		return false;
	
	auto from_ba = from_full_path.toLocal8Bit();
	int input_fd = ::open(from_ba.data(), O_RDONLY | O_LARGEFILE);
	
	if (input_fd == -1) {
		mtl_warn("%s: %s", from_ba.data(), strerror(errno));
		return false;
	}
	
	AutoCloseFd input_ac(input_fd);
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
	
	if (statx(0, from_ba.data(), flags, fields, &stx) != 0) {
		mtl_warn("%s", strerror(errno));
		return false;
	}
	
	auto to_full_path = (to_dir + name).toLocal8Bit();
	const auto OverwriteFlags = O_CREAT | O_TRUNC | O_LARGEFILE | O_WRONLY;
	int output_fd = ::open(to_full_path.data(), OverwriteFlags, stx.stx_mode);
	
	if (output_fd == -1) {
		mtl_status(errno);
		return false;
	}
	
	AutoCloseFd output_ac(output_fd);
	loff_t in_off = 0, out_off = 0;
	const usize chunk = 512 * 128;
	
	while (true) {
		isize count = copy_file_range(input_fd, &in_off, output_fd, &out_off, chunk, 0);
		if (count == -1) {
			if (errno == EAGAIN)
				continue;
			mtl_warn("%s => %s: %s", from_ba.data(), to_full_path.data(), strerror(errno));
			return false;
		} else if (count == 0) // zero means EOF
			break;
	}
	
	return true;
}

bool CountSizeRecursive(const QString &path, struct statx &stx,
	CountRecursiveInfo &info)
{
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	auto ba = path.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0) {
		mtl_warn("statx(): %s", strerror(errno));
		return -1;
	}
	
	const bool is_dir = S_ISDIR(stx.stx_mode);
	info.size += stx.stx_size;
	
	if (!is_dir) {
		info.size_without_dirs_meta += stx.stx_size;
		info.file_count++;
		return true;
	}
	
	info.dir_count++;
	QVector<QString> names;
	
	if (ListFileNames(path, names) != Err::Ok) {
		mtl_printq(path);
		return false;
	}
	
	for (const auto &name: names) {
		QString full_path = path;
		if (!full_path.endsWith('/'))
			full_path.append('/');
		full_path.append(name);
		
		if (!CountSizeRecursive(full_path, stx, info)) {
			mtl_printq2("Failed at full_path: ", full_path);
			return false;
		}
	}
	
	return true;
}

bool CountSizeRecursiveTh(const QString &path, struct statx &stx,
	CountFolderData &data)
{
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	auto ba = path.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0) {
		mtl_warn("statx(): %s", strerror(errno));
		data.Lock();
		QString msg = QChar('\"') + path + "\": " + strerror(errno);
		data.err = msg;
		data.Unlock();
		auto err_ba = msg.toLocal8Bit();
		mtl_warn("%s", err_ba.data());
		return -1;
	}
	
	const bool is_dir = S_ISDIR(stx.stx_mode);
	data.Lock();
		if (data.app_has_quit) {
			data.Unlock();
			return false;
		}
	
		data.info.size += stx.stx_size;
		
		if (!is_dir) {
			data.info.size_without_dirs_meta += stx.stx_size;
			data.info.file_count++;
			data.Unlock();
			return true;
		}
		
		data.info.dir_count++;
	data.Unlock();
	
	QVector<QString> names;
	
	if (ListFileNames(path, names) != Err::Ok) {
		mtl_printq(path);
		return false;
	}
	
	for (const auto &name: names) {
		QString full_path = path;
		if (!full_path.endsWith('/'))
			full_path.append('/');
		full_path.append(name);
		
		if (!CountSizeRecursiveTh(full_path, stx, data)) {
			return false;
		}
	}
	
	return true;
}

void Delete(io::File *file) {
	if (file->is_dir())
	{
		io::Files files;
		files.data.processed_dir_path = file->build_full_path() + '/';
		files.data.show_hidden_files(true);
		CHECK_TRUE_VOID((ListFiles(files.data, &files) == io::Err::Ok));
		
		for (io::File *next: files.data.vec) {
			if (next->is_dir())
				Delete(next);
			else
				next->DeleteFromDisk();
			delete next;
		}
	}
	file->DeleteFromDisk();
}

bool EnsureDir(const QString &dir_path, const QString &subdir)
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

bool ExpandLinksInDirPath(QString &unprocessed_dir_path, QString &processed_dir_path)
{
	QString dir_path = QDir::cleanPath(unprocessed_dir_path);
	
	if (dir_path == QLatin1String("/"))
	{
		processed_dir_path = dir_path;
		return true;
	}
	
	auto list = dir_path.splitRef('/', Qt::SkipEmptyParts);
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;//STATX_ALL;
	QString full_long_path;
	
	for (auto &item: list) {
		QString parent_dir = full_long_path;
		full_long_path += '/' + item;
		auto ba = full_long_path.toLocal8Bit();
		
		if (statx(0, ba.data(), flags, fields, &stx) != 0) {
			mtl_warn("statx(): %s", strerror(errno));
			return false;
		}
		
		if (S_ISLNK(stx.stx_mode)) {
			LinkTarget target;
			if (ReadLink(ba.data(), target, parent_dir)) {
				full_long_path = target.path;
			} else {
				return false;
			}
		}
	}
	
	if (!full_long_path.endsWith('/'))
		full_long_path.append('/');
	
	processed_dir_path = full_long_path;
	unprocessed_dir_path.clear();
	
	return true;
}

bool FileExistsCstr(const char *path, FileType *file_type)
{
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
	
	if (statx(0, path, flags, fields, &stx) != 0)
		return false;
	
	if (file_type != nullptr)
		*file_type = MapPosixTypeToLocal(stx.stx_mode);
	
	return true;
}

io::File*
FileFromPath(const QString &full_path, io::Err *ret_error)
{
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_ALL;
	QString name;
	QString dir_path;
	if (full_path != QLatin1String("/")) {
		int index = full_path.lastIndexOf('/');
		if (index == -1) {
			if (ret_error != nullptr)
				*ret_error = io::Err::Other;
			mtl_trace();
			return nullptr;
		}
		dir_path = full_path.mid(0, index + 1);
		name = full_path.mid(index + 1);
	} else {
		name = full_path;
	}
	
	auto ba = full_path.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0) {
		mtl_warn("statx(): %s", strerror(errno));
		if (ret_error != nullptr)
			*ret_error = MapPosixError(errno);
		return nullptr;
	}
	
	auto *file = new io::File(dir_path);
	FillInStx(*file, stx, &name);
	
	if (file->is_symlink()) {
		auto *target = new LinkTarget();
		ReadLink(ba.data(), *target, dir_path);
		file->link_target(target);
	}
	
	return file;
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

void FillInStx(io::File &file, const struct statx &stx, const QString *name)
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

void GetClipboardFiles(const QMimeData &mime, cornus::Clipboard &cl)
{
	cl.file_paths.clear();
	cl.action = ClipboardAction::None;
	cl.type = ClipboardType::None;
	
	QString text = mime.text();
	/// Need a regex because Nautilus in KDE inserts 'r' instead of just '\n'
	QRegularExpression regex("[\r\n]");
	auto list = text.splitRef(regex, Qt::SkipEmptyParts);
	const bool is_nautilus = text.startsWith(str::NautilusClipboardMime);
	
	if (is_nautilus)
	{
#ifdef CORNUS_CLIPBOARD_CLIENT_DEBUG
mtl_info("Nautilus style clipboard");
#endif
		cl.type = ClipboardType::Nautilus;
		if (list.size() < 3)
			return;
		
		for (int i = 2; i < list.size(); i++) {
			const QString s = list[i].toString();
			QUrl url(s);
			if (url.isLocalFile()) {
				cl.file_paths.append(url.toLocalFile());
			}
		}
		
		if (list[1] == QLatin1String("cut"))
			cl.action = ClipboardAction::Cut;
		else
			cl.action = ClipboardAction::Copy;
		return;
	}
#ifdef CORNUS_CLIPBOARD_CLIENT_DEBUG
mtl_info("KDE style clipboard");
#endif
	
	const QByteArray kde_ba = mime.data(str::KdeCutMime);
	const bool kde_cut_action = (!kde_ba.isEmpty() && kde_ba.at(0) == QLatin1Char('1'));
	
	if (kde_cut_action) {
		cl.type = ClipboardType::KDE;
	}
	
#ifdef CORNUS_CLIPBOARD_CLIENT_DEBUG
mtl_info("is cut: %s", kde_cut_action ? "true" : "false");
#endif
	for (const auto &next: list) {
		const QString s = next.toString();
#ifdef CORNUS_CLIPBOARD_CLIENT_DEBUG
mtl_printq(s);
#endif
		QUrl url(s);
		if (url.isLocalFile()) {
			cl.file_paths.append(url.toLocalFile());
		}
	}
	
	cl.action = kde_cut_action ? ClipboardAction::Cut : ClipboardAction::Copy;
}

QStringRef
GetFileNameExtension(const QString &name, QStringRef *base_name)
{
	int dot = name.lastIndexOf('.');
	
	if (dot == -1 || (dot == name.size() - 1))
		return QStringRef();
	
	if (base_name != nullptr) {
		*base_name = name.midRef(0, dot);
	}
	
	return name.midRef(dot + 1);
}

QStringRef
GetFileNameOfFullPath(const QString &full_path)
{
	// FullPath: "/home/user/my_dir/"
	bool already_found_letters = false;
	int skip_from_end = 0;
	int count = full_path.size();
	
	for (int i = count - 1; i >= 0; i--)
	{
		QChar c = full_path[i];
		if (c == '/') {
			if (already_found_letters) {
				int at = i + 1;
				return full_path.midRef(at, count - at - skip_from_end);
			} else {
				skip_from_end++;
			}
		} else {
			already_found_letters = true;
		}
	}
	
	return QStringRef();
}

bool IsNearlyEqual(double x, double y)
{
	const double epsilon = 1e-5;
	return std::abs(x - y) <= epsilon * std::abs(x);
	// see Knuth section 4.2.2 pages 217-218
}

Err ListFileNames(const QString &full_dir_path, QVector<QString> &vec)
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

Err ListFiles(io::FilesData &data, io::Files *ptr, FilterFunc ff)
{
	if (!data.unprocessed_dir_path.isEmpty()) {
		if (!ExpandLinksInDirPath(data.unprocessed_dir_path, data.processed_dir_path))
		{
			mtl_trace();
			return io::Err::Other;
		}
	}
	
	auto dir_path_ba = data.processed_dir_path.toLocal8Bit();
	DIR *dp = opendir(dir_path_ba.data());
	
	if (dp == NULL) {
		return MapPosixError(errno);
	}
	
	const bool hide_hidden_files = !data.show_hidden_files();
	struct dirent *entry;
	struct statx stx;
	
	while ((entry = readdir(dp)))
	{
		const char *n = entry->d_name;
		if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
			continue;
		
		QString name(n);
		
		if (hide_hidden_files && name.startsWith('.'))
			continue;
		
		if (ff != nullptr && !ff(data.processed_dir_path, name))
			continue;
		
		auto *file = new io::File(ptr);
		file->name(name);

		if (ReloadMeta(*file, stx, &data.processed_dir_path))
			data.vec.append(file);
		else
			delete file;
	}
	
	closedir(dp);
	std::sort(data.vec.begin(), data.vec.end(), cornus::io::SortFiles);
	
	return Err::Ok;
}

Err MapPosixError(int e)
{
	using io::Err;
	
	switch (e) {
	case EACCES: return Err::Access;
	case EIO: return Err::IO;
	default: return Err::Other;
	}
}

QString
NewNamePattern(const QString &filename, i32 &next)
{
	if (next == 0) {
		next++;
		return filename;
	}
	
	QStringRef base_name;
	const QStringRef ext = io::GetFileNameExtension(filename, &base_name);
	const QString num_str = QLatin1String(" (") + QString::number(next) + ')';
	next++;
	if (ext.isEmpty())
		return filename + num_str;
		
	return base_name + num_str + '.' + ext;
}

QString
PasteLinks(const QVector<QString> &full_paths, QString target_dir,
	QString *error)
{
	if (!target_dir.endsWith('/'))
		target_dir.append('/');
	
	QString first_successful;
	
	for (const QString &in_full_path: full_paths)
	{
		QString filename = io::GetFileNameOfFullPath(in_full_path).toString();
		if (filename.isEmpty())
			continue;
		
		auto target_path_ba = in_full_path.toLocal8Bit();
		i32 next = 0;
		while (true)
		{
			QString full_path = target_dir + io::NewNamePattern(filename, next);
			auto new_file_path = full_path.toLocal8Bit();
			int status = symlink(target_path_ba.data(), new_file_path.data());
			
			if (status == 0) {
				if (first_successful.isEmpty())
					first_successful = full_path;
				break;
			} else if (errno == EEXIST) {
				continue;
			} else {
				if (error != nullptr)
					*error = strerror(errno);
				break;
			}
		}
	}
	
	return first_successful;
}

void ProcessMime(QString &mime)
{
	const auto PlainText = QLatin1String("text/plain");
	
	if (mime.startsWith("text/"))
		mime = PlainText;
	
}

bool ReadLink(const char *file_path, LinkTarget &link_target, const QString &parent_dir)
{
	if (link_target.cycles == LinkTarget::MaxCycles) {
		link_target.cycles *= -1;
		mtl_warn("Symlink chain too large");
		return false;
	}
	
	link_target.chain_paths_.append(file_path);
	
	struct stat st;
	
	if (lstat(file_path, &st) == -1) {
		mtl_trace("lstat: %s, file: \"%s\"", strerror(errno), file_path);
		return false;
	}
	
	FileID file_id = FileID::New(st);
	if (link_target.chain_ids_.contains(file_id)) {
		link_target.cycles *= -1;
		return false;
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
		return false;
	}
	
	auto nbytes = readlink(file_path, buf, bufsize);
	
	if (nbytes == -1) {
		free(buf);
		mtl_trace("readlink: %s, file_path: \"%s\"", strerror(errno), file_path);
		return false;
	}
	
	/* If the return value was equal to the buffer size, then the
	the link target was larger than expected (perhaps because the
	target was changed between the call to lstat() and the call to
	readlink()). Warn the user that the returned target may have
	been truncated. */
	if (nbytes == bufsize) {
		mtl_warn("Returned buffer may have been truncated");
	}
	
	buf[nbytes] = 0;
	QString full_target_path = QString::fromLocal8Bit(buf, nbytes);
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
				return false;
			}
			QString parent_dir2 = dir.absolutePath();
			link_target.cycles++;
//			mtl_info("%s", ba.data());
//			mtl_printq2("parent_dir2: ", parent_dir2);
			return ReadLink(ba.data(), link_target, parent_dir2);
		}
	} else {
//		mtl_trace("lstat: %s, file: \"%s\"", strerror(errno), ba.data());
//		return;
	}
	
//	mtl_printq2("full_target_path: ", full_target_path);
	link_target.chain_paths_.append(full_target_path);
	link_target.path = full_target_path;
	link_target.mode = st.st_mode;
	link_target.type = MapPosixTypeToLocal(st.st_mode);
	
	return true;
}

bool ReadLinkSimple(const char *file_path, QString &result)
{
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	
	if (statx(0, file_path, flags, fields, &stx) == -1) {
		mtl_trace("lstat: %s, file: \"%s\"", strerror(errno), file_path);
		return false;
	}
	
	/* Add one to the link size, so that we can determine whether
	the buffer returned by readlink() was truncated. */
	i64 bufsize = stx.stx_size + 1;
	
	/* Some magic symlinks under (for example) /proc and /sys
	report 'st_size' as zero. In that case, take PATH_MAX as
	a "good enough" estimate. */
	if (stx.stx_size == 0)
		bufsize = PATH_MAX + 1;
	
	char *buf = (char*)malloc(bufsize);
	CHECK_PTR(buf);
	auto nbytes = readlink(file_path, buf, bufsize);
	
	if (nbytes == -1) {
		free(buf);
		mtl_trace("readlink: %s, file_path: \"%s\"", strerror(errno), file_path);
		return false;
	}
	
	/* If the return value was equal to the buffer size, then the
	the link target was larger than expected (perhaps because the
	target was changed between the call to lstat() and the call to
	readlink()). Warn the user that the returned target may have
	been truncated. */
	if (nbytes == bufsize) {
		mtl_warn("Returned buffer may have been truncated");
	}
	
	buf[nbytes] = 0;
	result = QString::fromLocal8Bit(buf, nbytes);
	
	return true;
}

io::Err ReadFile(const QString &full_path, cornus::ByteArray &buffer,
	const i64 read_max, mode_t *ret_mode)
{
	// This function follows links, it makes no sense not to
	// because I already show the link target in the table view.
	struct stat st;
	auto path = full_path.toLocal8Bit();
	
	if (stat(path.data(), &st) != 0)
		return MapPosixError(errno);
	
	if (ret_mode != nullptr)
		*ret_mode = st.st_mode;
	
	i64 MAX = (read_max == -1) ? st.st_size :
		std::min(read_max, st.st_size);
	
	if (MAX <= 0) {
		MAX = 1024 * 10;
	}
	
	buffer.make_sure(MAX);
	char *buf = buffer.data();
	const int fd = open(path.data(), O_RDONLY);
	
	if (fd == -1)
		return MapPosixError(errno);
	
	isize so_far = 0;
	
	while (so_far < MAX) {
		isize count = read(fd, buf + so_far, MAX - so_far);
		if (count == -1) {
			if (errno == EAGAIN)
				continue;
			buffer.size(so_far);
			io::Err e = MapPosixError(errno);
			mtl_warn("ReadFile: %s", strerror(errno));
			close(fd);
			return e;
		} else if (count == 0) {
			// zero indicates the end of file, happens with
			// virtual kernel generated files.
			break;
		}
		
		so_far += count;
	}
	
	close(fd);
	buffer.size(so_far);
	buffer.to(0);
	
	return Err::Ok;
}

void ReadXAttrs(io::File &file, const QByteArray &full_path)
{
	if (!file.can_have_xattr())
		return;
	
	isize buflen = llistxattr(full_path.data(), NULL, 0);
	if (buflen == 0)
		return; /// no attributes
	
	if (buflen == -1)
	{
		mtl_warn("%s: %s", full_path.data(), strerror(errno));
		return;
	}
	
	/// Allocate the buffer.
	char *buf = (char*)malloc(buflen);
	CHECK_PTR_VOID(buf);
	
	AutoFree af(buf);
	/// Copy the list of attribute keys to the buffer.
	buflen = llistxattr(full_path.data(), buf, buflen);
	CHECK_TRUE_VOID((buflen != -1));
	
	/** Loop over the list of zero terminated strings with the
		attribute keys. Use the remaining buffer length to determine
		the end of the list. */
	char *key = buf;
	QHash<QString, QString> &ext_attrs = file.ext_attrs();
	ext_attrs.clear();
	
	while (buflen > 0)
	{
		/// Determine length of the value.
		isize vallen = lgetxattr(full_path.constData(), key, NULL, 0);
		if (vallen <= 0)
			break;
		
		/// One extra byte is needed to append 0x00.
		char *val = (char*) malloc(vallen + 1);
		if (val == NULL)
		{
			mtl_status(errno);
			break;
		}
		AutoFree af_val(val);
		
		/// Copy value to buffer.
		vallen = lgetxattr(full_path.constData(), key, val, vallen);
		if (vallen == -1)
		{
			mtl_status(errno);
			break;
		}
		
		val[vallen] = 0;
		ext_attrs.insert(key, val);
		///mtl_info("Ext attr: \"%s\": \"%s\"", key, val);
		
		/// Forward to next attribute key.
		const isize keylen = strlen(key) + 1;
		buflen -= keylen;
		key += keylen;
	}
}

bool ReloadMeta(io::File &file, struct statx &stx, QString *dir_path)
{
	QByteArray full_path;
	if (dir_path != nullptr) {
		full_path = (*dir_path + file.name()).toLocal8Bit();
	} else {
		full_path = file.build_full_path().toLocal8Bit();
	}
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_ALL;
	
	if (statx(0, full_path.data(), flags, fields, &stx) != 0) {
		mtl_warn("statx(): %s: \"%s\"", strerror(errno), full_path.data());
		return false;
	}
	
	FillInStx(file, stx, nullptr);
	ReadXAttrs(file, full_path);
	
	if (file.is_symlink()) {
		auto *target = new LinkTarget();
		ReadLink(full_path.data(), *target,
			(dir_path != nullptr) ? *dir_path : file.dir_path());
		delete file.link_target();
		file.link_target(target);
	}
	
	return true;
}

bool SameFiles(const QString &path1, const QString &path2, io::Err *ret_error)
{
	struct statx stx;
	auto ba = path1.toLocal8Bit();
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_INO;
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0) {
		if (ret_error != nullptr)
			*ret_error = MapPosixError(errno);
		mtl_trace();
		return false;
	}
	
	auto id1 = FileID::NewStx(stx);
	ba = path2.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0) {
		if (ret_error != nullptr)
			*ret_error = MapPosixError(errno);
		mtl_trace();
		return false;
	}
	
	if (ret_error != nullptr)
		*ret_error = io::Err::Ok;
	
	auto id2 = FileID::NewStx(stx);
	return id1 == id2;
}

void InitEnvInfo(Category &desktop, QVector<QString> &search_icons_dirs,
QVector<QString> &xdg_data_dirs,
QHash<QString, Category> &possible_categories)
{
	xdg_data_dirs.clear();
	{
//		DConfClient *dc = dconf_client_new();
//		const gchar *p = "/org/gnome/desktop/interface/icon-theme";
//		GVariant *v = dconf_client_read(dc, p);
//		gchar *result;
//		g_variant_get (v, "s", &result);
//		theme_name_ = result;
//		g_free (result);
//		g_variant_unref(v);
	}
	
//	mtl_printq2("Theme name: ", theme_name_);
	
	category::InitAll(possible_categories);
	auto env = QProcessEnvironment::systemEnvironment();
	QString str = env.value(QLatin1String("XDG_CURRENT_DESKTOP")).toLower();
	desktop = possible_categories.value(str, Category::None);
//	auto ba = str.toLocal8Bit();
//	mtl_info("Desktop value: %u for: %s", (u8)desktop, ba.data());
	
	QString xdg_data_home = env.value(QLatin1String("XDG_DATA_HOME"));
	if (xdg_data_home.isEmpty())
		xdg_data_home = QDir::home().filePath(".local/share");
	
	{
		xdg_data_dirs.append(xdg_data_home);
		
		QString env_xdg_data_dirs = env.value(QLatin1String("XDG_DATA_DIRS"));
		
		if (env_xdg_data_dirs.isEmpty())
			env_xdg_data_dirs = QLatin1String("/usr/local/share/:/usr/share/");
		
		auto list = env_xdg_data_dirs.splitRef(':');
		
		for (const auto &s: list) {
			xdg_data_dirs.append(s.toString());
		}
	}
	
	{
		const QString icons = QLatin1String("icons");
		QString dir_path = QDir::homePath() + '/' + QLatin1String(".icons");
		
		if (io::FileExists(dir_path))
			search_icons_dirs.append(dir_path);
		
		for (const auto &xdg: xdg_data_dirs) {
			auto next = QDir(xdg).filePath(icons);
			
			if (io::FileExists(next))
				search_icons_dirs.append(next);
		}
		
		const char *usp = "/usr/share/pixmaps";
		if (io::FileExistsCstr(usp))
			search_icons_dirs.append(usp);
	}
}

QString
SizeToString(const i64 sz, const bool short_version)
{
	float rounded;
	QString type;
	if (sz >= io::TiB) {
		rounded = sz / io::TiB;
		type = short_version ? QLatin1String("T") : QLatin1String(" TiB");
	}
	else if (sz >= io::GiB) {
		rounded = sz / io::GiB;
		type = short_version ? QLatin1String("G") : QLatin1String(" GiB");
	} else if (sz >= io::MiB) {
		rounded = sz / io::MiB;
		type = short_version ? QLatin1String("M") : QLatin1String(" MiB");
	} else if (sz >= io::KiB) {
		rounded = sz / io::KiB;
		type = short_version ? QLatin1String("K") : QLatin1String(" KiB");
	} else {
		rounded = sz;
		type = short_version ? QLatin1String("B") : QLatin1String(" bytes");
	}
	
	return io::FloatToString(rounded, 1) + type;
}

int CompareDigits(QStringRef a, QStringRef b)
{
	int i = 0;
	for (; i < a.size(); i++) {
		if (a[i] != '0')
			break;
	}
	if (i != 0)
		a = a.mid(i);
	
	i = 0;
	for (; i < b.size(); i++) {
		if (b[i] != '0')
			break;
	}
	
	if (i != 0)
		b = b.mid(i);
	
	if (a.size() != b.size())
		return a.size() < b.size() ? -1 : 1;
	
	i = 0;
	for (; i < a.size(); i++)
	{
		const int an = a[i].digitValue();
		const int bn = b[i].digitValue();
		if (an != bn)
			return (an < bn) ? -1 : 1;
	}
	
	return 0;
}

QStringRef GetDigits(const QString &s, const int from)
{
	const int max = s.size();
	int k = from;
	for (; k < max; k++)
	{
		const QChar c = s[k];
		if (!c.isDigit())
			return s.midRef(from, k - from);
	}
	
	return s.midRef(from);
}

int CompareStrings(const QString &a, const QString &b)
{
/** Lexically compares this @a with @b and returns
 an integer less than, equal to, or greater than zero if @a
 is less than, equal to, or greater than the other string. */
	const int max = std::min(a.size(), b.size());
	for (int i = 0; i < max; i++)
	{
		const QChar ac = a[i];
		const QChar bc = b[i];
		
		if (ac.isDigit())
		{
			if (!bc.isDigit())
				return ac < bc ? -1 : 1;
			
			QStringRef a_digits = GetDigits(a, i);
			QStringRef b_digits = GetDigits(b, i);
			
			const int digit_result = CompareDigits(a_digits, b_digits);
//			if (true) {
//				auto ax = a_digits.toLocal8Bit();
//				auto bx = b_digits.toLocal8Bit();
//				mtl_info("\"%s\" vs \"%s\" = %d", ax.data(), bx.data(), digit_result);
//			}
			if (digit_result != 0)
				return digit_result;
			
			i += a_digits.size();
		}
		
		if (ac != bc)
			return ac < bc ? -1 : 1;
	}
	
	if (a.size() == b.size())
		return 0;
	
	return a.size() < b.size() ? -1 : 1;
}

bool SortFiles(io::File *a, io::File *b) 
{
/** Note: this function MUST be implemented with strict weak ordering
  otherwise it randomly crashes (because of undefined behavior),
  more info here:
 https://stackoverflow.com/questions/979759/operator-and-strict-weak-ordering/981299#981299 */
	const SortingOrder order = a->files()->data.sorting_order;
	
	if (a->is_dir_or_so() && !b->is_dir_or_so())
		return true;
	else if (b->is_dir_or_so() && !a->is_dir_or_so())
		return false;
	
	if (order.column == gui::Column::FileName) {
		///a->name_lower().compare(b->name_lower());
		int n = CompareStrings(a->name_lower(), b->name_lower());
		bool result = n >= 0 ? false : true;
		return order.ascending ? result : !result;
	}
	
	const bool by_created = (order.column == gui::Column::TimeCreated);
	if (by_created || (order.column == gui::Column::TimeModified))
	{
		auto &tc = by_created ? a->time_created() : a->time_modified();
		auto &tc2 = by_created ? b->time_created() : b->time_modified();
		
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
			int n = CompareStrings(a->name_lower(), b->name_lower());
			bool result = n >= 0 ? false : true;
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
			int n = CompareStrings(a->name_lower(), b->name_lower());
			bool result = n >= 0 ? false : true;
			return order.ascending ? result : !result;
		}
		
		const bool less_ext = a->cache().ext < b->cache().ext;
		return order.ascending ? less_ext : !less_ext;
	}
	
	mtl_trace();
	return false;
}

isize TryReadFile(const QString &full_path, char *buf, const i64 how_much,
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

io::Err WriteToFile(const QString &full_path, const char *data, const i64 size,
	mode_t *custom_mode)
{
	auto path = full_path.toLocal8Bit();
	const int fd = open(path.data(), O_LARGEFILE | O_WRONLY | O_CREAT | O_TRUNC,
		(custom_mode == nullptr) ? io::FilePermissions : *custom_mode);
	
	if (fd == -1)
		return MapPosixError(errno);
	
	i64 written = 0;
	i64 ret;
	
	while (written < size) {
		// ssize_t write(int fd, const void *buf, size_t count);
		ret = write(fd, data + written, size - written);
		
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



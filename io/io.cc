#include "io.hh"

#include "../AutoDelete.hh"
#include "../DesktopFile.hpp"
#include "../ExecInfo.hpp"
#include "../str.hxx"
#include "File.hpp"
#include "Files.hpp"
#include "../err.hpp"
#include "../ByteArray.hpp"
#include "SaveFile.hpp"
#include "../trash.hh"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <bits/stdc++.h> /// std::sort()
#include <sys/mman.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <zstd.h>


#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

namespace cornus::io {

const QString DesktopExt = QLatin1String("desktop");

void randname(char *buf)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

int create_shm_file(void)
{
	int retries = 100;
	do {
		char name[] = "/wl_shm-XXXXXX";
		randname(name + sizeof(name) - 7);
		--retries;
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);
	return -1;
}

int allocate_shm_file(size_t size)
{
	int fd = create_shm_file();
	if (fd < 0)
		return -1;
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

QString BuildTempPathFromID(const DiskFileId &id)
{
	QString s = io::GetLastingTmpDir();
	s.append(QString::number(id.dev_major)).append('_');
	s.append(QString::number(id.dev_minor)).append('_');
	s.append(QString::number(id.inode_number));
	
	return s;
}

bool CanWriteToDir(QStringView dir_path)
{
	auto ba = dir_path.toLocal8Bit();
	return access(ba.data(), W_OK) == 0;
}

bool CheckDesktopFileABI(ByteArray &ba)
{
	return ba.has_more(sizeof(DesktopFileABI)) && ba.next_i16() == DesktopFileABI;
}

int CompareDigits(QStringView a, QStringView b)
{
	int i = 0;
	for (; i < a.size(); i++) {
		if (a[i] != '0')
			break;
	}
	if (i != 0)
		a = a.sliced(i);
	
	i = 0;
	for (; i < b.size(); i++) {
		if (b[i] != '0')
			break;
	}
	
	if (i != 0)
		b = b.sliced(i);
	
	if (a.size() != b.size())
		return a.size() < b.size() ? -1 : 1;
	
	i = 0;
	for (; i < a.size(); i++)
	{
		cint an = a[i].digitValue();
		cint bn = b[i].digitValue();
		if (an != bn)
			return (an < bn) ? -1 : 1;
	}
	
	return 0;
}

int CountDirFilesSkippingSubdirs(QStringView dir_path)
{
	// This function is used to check if the folder has any files.
	struct dirent *entry;
	auto dir_path_ba = dir_path.toLocal8Bit();
	DIR *dp = opendir(dir_path_ba.data());
	
	if (dp == NULL)
		return -errno; // errno is positive, hence negating it.
	
	int count = 0;
	while ((entry = readdir(dp)))
	{
		const char *n = entry->d_name;
		if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
			continue;
		
		count++;
	}
	
	closedir(dp);
	
	return count;
}

media::MediaPreview* CreateMediaPreview(ByteArray &media_data)
{
	cauto at = media_data.at();
	if (media_data.size() < sizeof(i32)) {
		mtl_trace();
		return nullptr;
	}
	
	media::MediaPreview *preview = new media::MediaPreview();
	preview->magic_number = media_data.next_i32();
	
	while (media_data.has_more())
	{
		// mtl_info();
		media::Field f = (media::Field) media_data.next_u8();
		QVector<i32> *v32 = nullptr;
		
		if (f == media::Field::Actors) {
			v32 = &preview->actors;
			// mtl_info("v32 actors");
		} else if (f == media::Field::Directors) {
			v32 = &preview->directors;
			// mtl_info("v32 directors");
		} else if (f == media::Field::Writers) {
			v32 = &preview->writers;
			// mtl_info("v32 writers");
		}
		
		if (v32 != nullptr)
		{
			const u16 count = media_data.next_u16();
			for (int i = 0; i < count; i++) {
				v32->append(media_data.next_i32());
			}
			continue;
		}
		
		QVector<i16> *v2 = nullptr;
		
		if (f == media::Field::Genres) {
			v2 = &preview->genres;
			// mtl_info("v2 genres");
		} else if (f == media::Field::Subgenres) {
			v2 = &preview->subgenres;
			// mtl_info("v2 subgenres");
		} else if (f == media::Field::Countries) {
			v2 = &preview->countries;
			// mtl_info("v2 countries");
		} else if (f == media::Field::Rip) {
			v2 = &preview->rips;
			// mtl_info("v2 rips");
		} else if (f == media::Field::VideoCodec) {
			v2 = &preview->video_codecs;
			// mtl_info("v2 video_codecs");
		}
		
		if (v2 != nullptr)
		{
			cu16 count = media_data.next_u16();
			// mtl_info("Count: %u", u32(count));
			for (int i = 0; i < count; i++) {
				v2->append(media_data.next_i16());
			}
			// mtl_info("Done");
			continue;
		}
		
		if (f == media::Field::YearStarted) {
			preview->year_started = media_data.next_i16();
		} else if (f == media::Field::MonthStarted) {
			preview->month_started = media_data.next_i8();
		} else if (f == media::Field::DayStarted) {
			preview->day_started = media_data.next_i8();
		} else if (f == media::Field::YearEnded) {
			preview->year_end = media_data.next_i16();
		} else if (f == media::Field::VideoCodecBitDepth) {
			preview->bit_depth = media_data.next_i16();
		} else if (f == media::Field::VideoResolution) {
			preview->video_w = media_data.next_i32();
			preview->video_h = media_data.next_i32();
		} else if (f == media::Field::FPS) {
			preview->fps = media_data.next_f32();
		} else {
			/// other fields not needed by media::MediaPreview
		}
	}
	
	media_data.to(at);
	return preview;
}

bool CreateRegularFile(QStringView full_path)
{
	QFile file(full_path.toString());
	return file.open(QIODevice::WriteOnly);
}

QStringView GetDigits(QStringView s, cint from)
{
	cint max = s.size();
	int k = from;
	for (; k < max; k++)
	{
		const QChar c = s[k];
		if (!c.isDigit())
			return s.sliced(from, k - from);
	}
	
	return s.sliced(from);
}

int CompareStrings(QStringView a, QStringView b)
{
/** Lexically compares this @a with @b and returns
 an integer less than, equal to, or greater than zero if @a
 is less than, equal to, or greater than the other string. */
	cint max = std::min(a.size(), b.size());
	for (int i = 0; i < max; i++)
	{
		const QChar ac = a[i];
		const QChar bc = b[i];
		
		if (ac.isDigit())
		{
			if (!bc.isDigit())
				return ac < bc ? -1 : 1;
			
			auto a_digits = GetDigits(a, i);
			auto b_digits = GetDigits(b, i);
			
			cint digit_result = CompareDigits(a_digits, b_digits);
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

bool CopyFileFromTo(QStringView from_full_path, QString to_dir)
{
	if (!to_dir.endsWith('/'))
		to_dir.append('/');
	
	auto name = io::GetFileNameOfFullPath(from_full_path);
	
	if (name.isEmpty())
		return false;
	
	auto from_ba = from_full_path.toLocal8Bit();
	cint input_fd = ::open(from_ba.data(), O_RDONLY | O_LARGEFILE);
	
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
	
	auto to_full_path = (to_dir + name.toString()).toLocal8Bit();
	const auto OverwriteFlags = O_CREAT | O_TRUNC | O_LARGEFILE | O_WRONLY;
	int output_fd = ::open(to_full_path.data(), OverwriteFlags, stx.stx_mode);
	
	if (output_fd == -1) {
		mtl_status(errno);
		return false;
	}
	
	AutoCloseFd output_ac(output_fd);
	loff_t in_off = 0, out_off = 0;
	cusize chunk = 512 * 128;
	
	while (true) {
		cisize count = copy_file_range(input_fd, &in_off, output_fd, &out_off, chunk, 0);
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
	CountRecursiveInfo &info, const FirstTime ft)
{
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	auto ba = path.toLocal8Bit();
	if (statx(0, ba.data(), flags, fields, &stx) != 0)
	{
		mtl_warn("statx(): %s", strerror(errno));
		return false;
	}
	
	cbool is_dir = S_ISDIR(stx.stx_mode);
	info.size += stx.stx_size;
	if (!is_dir)
	{
		info.file_count++;
		return true;
	}
	
	if (ft == FirstTime::No) // don't count top dir itself
		info.dir_count++;
	
	QVector<QString> names;
	if (!ListFileNames(path, names))
	{
		mtl_printq(path);
		return false;
	}
	
	for (const auto &name: names)
	{
		QString full_path = path;
		if (!full_path.endsWith('/'))
			full_path.append('/');
		full_path.append(name);
		
		if (!CountSizeRecursive(full_path, stx, info, FirstTime::No))
		{
			mtl_printq2("Failed at full_path: ", full_path);
			return false;
		}
	}
	
	return true;
}

int CreateAutoRenamedFile(QString dir_path, QString filename,
	cint file_flags, const mode_t mode)
{
	int next = 0;
	while(true)
	{
		QString dest_path = dir_path + io::NewNamePattern(filename, next);
		auto ba = dest_path.toLocal8Bit();
		cint fd = ::open(ba.data(), file_flags, mode);
		if (fd != -1)
			return fd;
		if (errno != EEXIST)
			return -1;
	}
	
	return 0;
}

int DeleteFolder(QString dp, const DeleteSubFolders dsf, const DeleteTopFolder dtf)
{
	if (dsf == DeleteSubFolders::No && dtf == DeleteTopFolder::Yes) {
		mtl_warn("Can't delete top folder without deleting all of its contents");
		return EINVAL;
	}
	
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
	struct statx stx;
	
	QVector<QString> filenames;
	if (!ListFileNames(dp, filenames))
		return EACCES;
	
	if (!dp.endsWith('/'))
		dp.append('/');
	
	for (const QString &name: filenames)
	{
		QString full_path = dp + name;
		auto ba = full_path.toLocal8Bit();
		if (statx(0, ba.data(), flags, fields, &stx) == -1)
		{
			mtl_warn("%s: %s", ba.data(), strerror(errno));
			return errno;
		}
		
		if (S_ISDIR(stx.stx_mode)) {
			if (dsf == DeleteSubFolders::Yes) {
				cint status = DeleteFolder(full_path);
				if (status != 0)
					return status;
			}
		} else {
			if(::remove(ba.data()) == -1) {
				return errno;
			}
		}
	}
	
	if (dtf == DeleteTopFolder::Yes) {
		auto dir_ba = dp.toLocal8Bit();
		return (::remove(dir_ba.data()) == -1) ? errno : 0;
	}
	
	return 0;
}

int DoStat(const QString &full_path, const QString &name, bool &is_trash_dir,
	cbool do_check, CountFolderData &data)
{
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_SIZE | STATX_MODE;
	auto path_ba = full_path.toLocal8Bit();
	
	if (statx(0, path_ba.data(), flags, fields, &data.stx) == 0)
	{
		if (do_check && S_ISDIR(data.stx.stx_mode)) {
			is_trash_dir = name.startsWith(trash::basename());
			if (is_trash_dir) {
				auto guard = data.guard();
				data.info.trash_dir_count--; // don't count the trash can itself
			}
		}
		return 0;
	}

	mtl_warn("statx(): %s", strerror(errno));
	return errno;
}

int CountSizeRecursiveTh(const QString &path,
	CountFolderData &data, cbool inside_trash)
{
	cbool is_dir = S_ISDIR(data.stx.stx_mode);
	{
		auto guard = data.guard();
		if (data.app_has_quit)
			return 0;
		
		if (inside_trash)
			data.info.trash_size += data.stx.stx_size;
		data.info.size += data.stx.stx_size;
		
		if (!is_dir) {
			data.info.file_count++;
			if (inside_trash)
				data.info.trash_file_count++;
			return 0;
		}
		
		data.info.dir_count++;
		if (inside_trash)
			data.info.trash_dir_count++;
	}
	
	QVector<QString> names;
	
	if (!ListFileNames(path, names)) {
		mtl_printq(path);
		return errno;
	}
	
	for (const auto &name: names)
	{
		QString full_path = path;
		if (!full_path.endsWith('/'))
			full_path.append('/');
		full_path.append(name);
		
		bool is_trash_dir = false;
		int err = io::DoStat(full_path, name, is_trash_dir, !inside_trash, data);
		if (err != 0)
			return err;
		
		cbool inside_trash2 = inside_trash ? inside_trash : is_trash_dir;
		err = CountSizeRecursiveTh(full_path, data, inside_trash2);
		if (err != 0)
			return err;
	}
	
	return 0;
}

void Delete(io::File *file, const QProcessEnvironment &env)
{
	if (file->is_dir())
	{
		io::Files files;
		files.data.processed_dir_path = file->build_full_path() + '/';
		files.data.show_hidden_files(true);
		MTL_CHECK_VOID(ListFiles(files.data, &files, env, CountDirFiles::No));
		
		for (io::File *next: files.data.vec) {
			if (next->is_dir())
				Delete(next, env);
			else
				next->Delete();
			delete next;
		}
	}
	file->Delete();
}

bool DirExists(const QString &full_path)
{
	auto ba = full_path.toLocal8Bit();
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0)
		return false;
	
	return (S_ISDIR(stx.stx_mode));
}

DirType GetDirType(const QString &full_path)
{
	auto ba = full_path.toLocal8Bit();
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0)
		return DirType::Error;
	
	if (S_ISDIR(stx.stx_mode))
		return DirType::Dir;
	
	if (!S_ISLNK(stx.stx_mode))
		return DirType::Neither;
	
	LinkTarget target;
	if (!io::ReadLink(ba.data(), target))
		return DirType::Error;
	
	return (target.type == FileType::Dir) ? DirType::LinkToDir : DirType::Neither;
}

bool EnsureDir(QString dir_path, const QString &subdir, QString *result)
{
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	dir_path.append(subdir);
	auto ba = dir_path.toLocal8Bit();
	FileType ft;
	
	if (FileExistsCstr(ba.data(), &ft))
	{
		if (ft == FileType::Dir) {
			if (result)
				*result = dir_path;
			return true;
		}
		
		if (remove(ba.data()) != 0)
			return false;
	}
	
	cint status = mkdir(ba.data(), DirPermissions);
	if (status == 0)
	{
		if (result)
			*result = dir_path;
		return true;
	}
	
	return false;
}

bool EnsureRegularFile(const QString &full_path, const mode_t *mode)
{
	const QByteArray ba = full_path.toLocal8Bit();
	FileType t;
	if (FileExistsCstr(ba.data(), &t))
	{
		if (t == FileType::Regular)
			return true;
		
		if (t == FileType::Dir)
		{
			MTL_CHECK(rmdir(ba.data()) == 0);
		} else {
			MTL_CHECK(unlink(ba.data()) == 0);
		}
	}
	
	const mode_t file_mode = (mode) ? *mode : 0644;
	const auto OverwriteFlags = O_CREAT | O_LARGEFILE;
	cint output_fd = ::open(ba.data(), OverwriteFlags, file_mode);
	if (output_fd == -1)
	{
		mtl_status(errno);
		return false;
	}
	
	::close(output_fd);
	return true;
}

bool ExpandLinksInDirPath(QString &unprocessed_dir_path,
	QString &processed_dir_path, const AppendSlash afs)
{
	QString dir_path = QDir::cleanPath(unprocessed_dir_path);
	
	if (dir_path == QLatin1String("/"))
	{
		processed_dir_path = dir_path;
		return true;
	}
	
	auto list = dir_path.split('/', Qt::SkipEmptyParts);
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
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
	
	if (afs == AppendSlash::Yes) {
		if (!full_long_path.endsWith('/'))
			full_long_path.append('/');
	}
	
	processed_dir_path = full_long_path;
	
	return true;
}

bool FileContentsContains(const QString &full_path,
	const QString &searched_str)
{
	io::ReadParams read_params = {};
	read_params.print_errors = PrintErrors::Yes;
	ByteArray buf;
	MTL_CHECK(io::ReadFile(full_path, buf, read_params));
	QString s = buf.toString();
	
	return s.contains(searched_str);
}

bool FileExistsCstr(const char *path, FileType *file_type)
{
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = (file_type == nullptr) ? 0 : STATX_MODE;
	
	if (statx(0, path, flags, fields, &stx) != 0)
		return false;
	
	if (file_type != nullptr)
		*file_type = MapPosixTypeToLocal(stx.stx_mode);
	
	return true;
}

io::File*
FileFromPath(const QString &full_path, int *ret_error)
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
				*ret_error = EINVAL;
			return nullptr;
		}
		dir_path = full_path.mid(0, index + 1);
		name = full_path.mid(index + 1);
	} else {
		name = full_path;
	}
	
	auto ba = full_path.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0) {
		mtl_warn("statx(): %s: %s", strerror(errno), ba.data());
		if (ret_error != nullptr)
			*ret_error = errno;
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

void FillInStx(io::File &file, const struct statx &stx, const QString *name)
{
	using io::FileType;
	if (name != nullptr)
		file.name(*name);
	file.mode(stx.stx_mode);
	file.size(stx.stx_size);
	file.type(MapPosixTypeToLocal(stx.stx_mode));
	file.id(io::DiskFileId::FromStx(stx));
	file.time_created(stx.stx_btime);
	file.time_modified(stx.stx_mtime);
}

QString
FloatToString(const float number, cint precision)
{
	float rem = i32(number) - number;

	if (IsNearlyEqual(rem, 0.0))
		return QString::number(i32(number));
	
	return QString::number(number, 'f', precision);
}

QStringView
GetFileNameExtension(QStringView name, QStringView *base_name)
{
	cint dot = name.lastIndexOf('.');
	
	if (dot == -1 || (dot == name.size() - 1))
		return {};
	
	if (base_name) {
		*base_name = name.sliced(0, dot);
	}
	
	return name.sliced(dot + 1);
}

QStringView
GetFileNameOfFullPath(QStringView full_path)
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
				return full_path.sliced(at, count - at - skip_from_end);
			} else {
				skip_from_end++;
			}
		} else {
			already_found_letters = true;
		}
	}
	
	return {};
}

QString get_lasting_tmp_dir()
{
	QString s;
	if (!EnsureDir(QLatin1String("/var/tmp/"), QLatin1String("Cornus.Mas"), &s))
	{
		mtl_trace();
		return QString();
	}
	
	if (!s.endsWith('/'))
		s.append('/');
	
	return s;
}

QString GetLastingTmpDir()
{
	static QString s = get_lasting_tmp_dir();
	return s;
}

QStringView GetParentDirPath(QStringView full_path)
{
	int at = full_path.size() - 1;
	
	while ((at > 0) && (full_path.at(at) == '/')) {
		at--;
	}
	
	at--;
	
	while ((at >= 0) && (full_path.at(at) != '/')) {
		at--;
	}
	
	if (at < 0) {
		return {};
	}
	
	if (at == 0)
		at = 1;
	
	return full_path.sliced(0, at);
}

Bool HasExecBit(const QString &full_path)
{
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
	auto ba = full_path.toLocal8Bit();
	if (statx(0, ba.data(), flags, fields, &stx) == -1) {
		return Bool::None;
	}
	
	return (stx.stx_mode & io::ExecBits) ? Bool::Yes : Bool::No;
}

void InitEnvInfo(Category &desktop, QVector<QString> &search_icons_dirs,
QVector<QString> &xdg_data_dirs, QHash<QString, Category> &possible_categories,
	QProcessEnvironment &env)
{
	xdg_data_dirs.clear();
	category::InitAll(possible_categories);
	QString str = env.value(QLatin1String("XDG_CURRENT_DESKTOP")).toLower();
	///workaround Ubuntu's "ubuntu:GNOME" $XDG_CURRENT_DESKTOP value.
	if (str.indexOf(str::Gnome) != -1) {
		desktop = Category::Gnome;
	} else {
		desktop = possible_categories.value(str, Category::None);
	}
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
		
		auto list = env_xdg_data_dirs.split(':');
		
		for (const auto &s: list) {
			xdg_data_dirs.append(s);
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

bool IsNearlyEqual(double x, double y)
{
	const double epsilon = 1e-5;
	return std::abs(x - y) <= epsilon * std::abs(x);
	// see Knuth section 4.2.2 pages 217-218
}

int ListDirNames(QString dir_path, QVector<QString> &vec, const ListDirOption option)
{
	struct dirent *entry;
	auto dir_path_ba = dir_path.toLocal8Bit();
	DIR *dp = opendir(dir_path_ba.data());
	
	if (dp == NULL)
		return errno;
	
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_MODE;
	
	while ((entry = readdir(dp)))
	{
		const char *n = entry->d_name;
		if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
			continue;
		
		QString name(n);
		auto ba = (dir_path + name).toLocal8Bit();
		
		if (statx(0, ba.data(), flags, fields, &stx) != 0) {
			///mtl_warn("statx(): %s: \"%s\"", strerror(errno), ba.data());
			continue;
		}
		
		if (S_ISDIR(stx.stx_mode))
			vec.append(name);
		else if (option == ListDirOption::IncludeLinksToDirs && S_ISLNK(stx.stx_mode))
		{
			LinkTarget target;
			ReadLink(ba.data(), target, dir_path);
			if (S_ISDIR(target.mode))
				vec.append(name);
		}
	}
	
	closedir(dp);
	
	return 0;
}

bool ListFileNames(QStringView full_dir_path, QVector<QString> &vec,
	FilterFunc ff)
{
	struct dirent *entry;
	auto dir_path_ba = full_dir_path.toLocal8Bit();
	DIR *dp = opendir(dir_path_ba.data());
	
	if (dp == NULL)
		return false;
	
	QString dir_path = full_dir_path.toString();
	
	if (!dir_path.endsWith('/'))
		dir_path.append('/');
	
	while ((entry = readdir(dp)))
	{
		const char *n = entry->d_name;
		if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0)
			continue;
		
		QString name(n);
		if (ff && !ff(name))
			continue;
		
		vec.append(name);
	}
	
	closedir(dp);
	
	return true;
}

bool ListFiles(io::FilesData &data, io::Files *ptr,
	const QProcessEnvironment &env,
	const CountDirFiles cdf,
	const QHash<QString, Category> *possible_categories,
	FilterFunc ff)
{
	if (!data.unprocessed_dir_path.isEmpty()) {
		if (!ExpandLinksInDirPath(data.unprocessed_dir_path, data.processed_dir_path))
			return false;//EINVAL;
	}
	
	{ // this line is needed for file->CountDirFiles1Level()
		// to work properly, it's called later in this function.
		ptr->data.processed_dir_path = data.processed_dir_path;
	}
	
	auto dir_path_ba = data.processed_dir_path.toLocal8Bit();
	DIR *dp = opendir(dir_path_ba.data());
	
	if (dp == NULL)
		return false;//errno;
	
	cbool hide_hidden_files = !data.show_hidden_files();
	struct dirent *entry;
	struct statx stx;
	const QString dot = QLatin1String(".");
	const QString two_dots = QLatin1String("..");
	
	while ((entry = readdir(dp)))
	{
		const QString name(entry->d_name);
		if (name == dot || name == two_dots)
			continue;
		
		if (hide_hidden_files && name.startsWith('.'))
			continue;
		
		if (ff && !ff(name))
			continue;
		
		auto *file = new io::File(ptr);
		file->name(name);
		file->cache().possible_categories = possible_categories;

		if (ReloadMeta(*file, stx, env, PrintErrors::Yes, &data.processed_dir_path))
		{
			data.vec.append(file);
			if (cdf == CountDirFiles::Yes && file->is_dir_or_so())
				file->CountDirFiles();
		} else {
			delete file;
		}
	}
	
	closedir(dp);
	
	cbool can_write = access(dir_path_ba.data(), W_OK) == 0;
	data.can_write_to_dir(can_write);
	std::sort(data.vec.begin(), data.vec.end(), cornus::io::SortFiles);
	
	return true;
}

QString MergeList(QStringList list, QChar delim)
{
	QString s;
	for (auto &next: list) {
		s.append(next + delim);
	}
	
	return s;
}

QString NewNamePattern(QStringView filename, i32 &next)
{
	QStringView base_name;
	QStringView ext = io::GetFileNameExtension(filename, &base_name);
	QString num_str = QLatin1String(" (") + QString::number(++next) + ')';
	if (ext.isEmpty())
		return filename.toString() + num_str;
	
	return base_name.toString() + num_str + '.' + ext.toString();
}

void PasteLinks(const QList<QUrl> &urls,
	QString target_dir, QVector<QString> *filenames, QString *error)
{
	if (!target_dir.endsWith('/'))
		target_dir.append('/');
	
	for (cauto &url: urls)
	{
		QString in_full_path = url.toLocalFile();
		QString filename = io::GetFileNameOfFullPath(in_full_path).toString();
		if (filename.isEmpty())
			continue;
		
		if (filenames)
			filenames->append(filename);
		
		auto target_path_ba = in_full_path.toLocal8Bit();
		i32 next = 0;
		while (true)
		{
			QString full_path = target_dir + io::NewNamePattern(filename, next);
			auto new_file_path = full_path.toLocal8Bit();
			int status = symlink(target_path_ba.data(), new_file_path.data());
			
			if (status == 0) {
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
}

void PasteRelativeLinks(const QList<QUrl> &full_paths,
	QString target_dir, QVector<QString> *filenames, QString *error)
{
	if (!target_dir.endsWith('/'))
		target_dir.append('/');
	
	for (cauto &url: full_paths)
	{
		const QString full_path = url.toLocalFile();
		QString filename = io::GetFileNameOfFullPath(full_path).toString();
		if (filename.isEmpty()) {
			mtl_trace();
			continue;
		}
		if (filenames)
			filenames->append(filename);
		
		QString target = full_path;
		if (target.startsWith(target_dir)) {
			target = target.mid(target_dir.size());
		}
		auto target_path_ba = target.toLocal8Bit();
		i32 next = 0;
		while (true)
		{
			QString full_path = target_dir + io::NewNamePattern(filename, next);
			auto link_path_ba = full_path.toLocal8Bit();
			
			int status = symlink(target_path_ba.data(), link_path_ba.data());
			if (status == 0) {
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
	
}

QString PrepareTestingFolder(QStringView subdir)
{ // special folder to carry out inotify tests of this app
	QString test_dir;
	if (!cornus::io::EnsureDir(QDir::homePath(), subdir.toString(), &test_dir))
		return QString();
	
	cint status = io::DeleteFolder(test_dir, DeleteSubFolders::Yes, DeleteTopFolder::No);
	if (status != 0) {
		mtl_status(status);
		return QString();
	}
	
	return test_dir;
}

void ProcessMime(QString &mime)
{
	const auto PlainText = QLatin1String("text/plain");
	
	if (mime.startsWith("text/"))
		mime = PlainText;
	
}

const char* QuerySocketFor(const QString &dir_path, bool &needs_root)
{
	auto ba = dir_path.toLocal8Bit();
	needs_root = (access(ba.data(), W_OK) != 0);
	//mtl_info("Needs root: %s %s", (needs_root ? "yes" : "no"), qPrintable(dir_path));
	if (true)
	{
		// disable cornus_r for now
		needs_root = false;
//		mtl_trace("Reminder");
	}
	
	return needs_root ? cornus::RootSocketPath : cornus::SocketPath;
}

i64 ReadEventFd(cint fd)
{
	i64 n;
	if (read(fd, &n, sizeof n) >= sizeof n)
		return n;
	
	mtl_trace();
	return -1;
}

bool ReadFile(const QString &full_path, cornus::ByteArray &buffer,
	const ReadParams &params)
{
	struct statx stx;
	auto path = full_path.toLocal8Bit();
	const auto flags = 0;// this function must follow symlinks
	const auto fields = STATX_MODE | STATX_SIZE;
	bool statx_ok;
	if (statx(0, path.data(), flags, fields, &stx) != 0)
	{
		if (params.print_errors == PrintErrors::Yes)
			mtl_warn("%s: \"%s\"", strerror(errno), path.data());
		if (params.ret_mode != nullptr)
			*(params.ret_mode) = 0;
		
		statx_ok = false;
	} else {
		statx_ok = true;
		if (params.ret_mode != nullptr)
			*(params.ret_mode) = stx.stx_mode;
	}
	
	cint fd = ::open(path.data(), O_RDONLY);
	if (fd == -1)
		return false;
	
	AutoCloseFd acf(fd);
	cauto at = buffer.at();
	ExactSize es;
	if (statx_ok && params.can_rely == CanRelyOnStatxSize::Yes)
	{
		es = ExactSize::Yes;
		buffer.MakeSure(stx.stx_size, ExactSize::Yes);
	} else {
		es = ExactSize::No;
	}
	
	i64 so_far = 0;
	cisize buf_size = 4096 * 4;
	char *buf = new char[buf_size];
	AutoDeleteArr buf_(buf);
	while (true)
	{
		ci64 actually_read = read(fd, buf, buf_size);
		if (actually_read == -1)
		{
			if (errno == EAGAIN)
				continue;
			if (params.print_errors == PrintErrors::Yes)
				mtl_warn("ReadFile: %s", strerror(errno));
			return false;
		} else if (actually_read == 0) {
			/// Zero indicates the end of file, happens with sysfs files.
			break;
		}
		
		so_far += actually_read;
		
		if (params.read_max != -1 && so_far >= params.read_max)
		{
			so_far -= actually_read;
			break;
		}
		
		buffer.add(buf, actually_read, es);
	}
	
	buffer.to(at);
	buffer.size(at + so_far); /// needed for buffer.toString()
	
	return true;
}

bool ReadLink(const char *file_path, LinkTarget &link_target,
	const QString &parent_dir, const PrintErrors pe)
{
	if (link_target.cycles == LinkTarget::MaxCycles)
	{
		link_target.cycles *= -1;
		mtl_warn("Symlink chain too large");
		return false;
	}
	
	link_target.chain_paths_.append(file_path);
	struct statx stx;
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_TYPE | STATX_MODE | STATX_SIZE;
	if (statx(0, file_path, flags, fields, &stx) != 0)
	{
		if (pe == PrintErrors::Yes)
			mtl_warn("statx(): %s: \"%s\"", strerror(errno), file_path);
		return false;
	}
	
	DiskFileId file_id = DiskFileId::FromStx(stx);
	if (link_target.chain_ids_.contains(file_id)) {
		link_target.cycles *= -1;
		return false;
	} else {
		link_target.chain_ids_.append(file_id);
	}
	
	/* Add one to the link size, so that we can determine whether
	the buffer returned by readlink() was truncated. */
	i64 bufsize = stx.stx_size + 1;
	
	/* Some magic symlinks under (for example) /proc and /sys
	report size=0. In that case, take PATH_MAX as a "good enough" estimate. */
	if (stx.stx_size == 0)
		bufsize = PATH_MAX;
	
	char *buf = (char*)malloc(bufsize);
	if (buf == NULL)
	{
		mtl_trace("malloc: %s", strerror(errno));
		return false;
	}
	
	auto nbytes = readlink(file_path, buf, bufsize);
	if (nbytes == -1)
	{
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
	bool is_relative = false;
	if (!full_target_path.startsWith('/'))
	{
		is_relative = true;
		QString s = parent_dir;
		
		if (s.isEmpty())
		{
			QString path_str = file_path;
			auto s = io::GetParentDirPath(path_str);
			if (s.isEmpty())
			{
				free(buf);
				return false;
			}
		}
		
		if (!s.endsWith('/'))
			s.append('/');
		
		s.append(full_target_path);
		full_target_path = s;
	}
	
	free(buf);
	buf = NULL;
	full_target_path = QDir::cleanPath(full_target_path);
	auto ba = full_target_path.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) == 0)
	{
		if ((stx.stx_mode & S_IFMT) == S_IFLNK)
		{
			QDir dir(full_target_path);
			if (!dir.cdUp())
			{
				mtl_trace();
				return false;
			}
			
			QString parent_dir2 = dir.absolutePath();
			link_target.cycles++;
			
			return ReadLink(ba.data(), link_target, parent_dir2);
		}
	}
	
	link_target.chain_paths_.append(full_target_path);
	link_target.path = full_target_path;
	link_target.is_relative = is_relative;
	link_target.mode = stx.stx_mode;
	link_target.type = MapPosixTypeToLocal(stx.stx_mode);
	
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
	MTL_CHECK(buf != nullptr);
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

i64 ReadToBuf(cint fd, char *buf, ci64 buf_size,
	const PrintErrors pe)
{
	i64 so_far = 0;
	while (true)
	{
//mtl_info("Starting ::read()");
		cisize chunk = ::read(fd, buf + so_far, buf_size - so_far);
//mtl_info("Read %ld", chunk);
		if (chunk == -1)
		{
			if (errno == EAGAIN)
				continue;
			if (pe == PrintErrors::Yes)
				mtl_warn("ReadFile: %s", strerror(errno));
			return -1;
		} else if (chunk == 0) {
			/// Zero indicates the end of file, happens with sysfs files.
			break;
		}
		
		so_far += chunk;
	}
	
	return so_far;
}

void ReadXAttrs(io::File &file, const QByteArray &full_path)
{
	if (!file.can_have_xattr()) {
		mtl_warn("can't have xattr: %s", qPrintable(file.name()));
		return;
	}

	file.ClearXAttrs();
	QHash<QString, ByteArray> &ext_attrs = file.ext_attrs();
	
	isize buflen = llistxattr(full_path.data(), NULL, 0);
	if (buflen <= 0) {
		return; /// 0 = no attributes, -1 = error
	}
	
	/// Allocate the buffer.
	char *buf = new char[buflen];
	MTL_CHECK_VOID(buf != nullptr);
	AutoDeleteArr ad(buf);
	
	/// Copy the list of attribute keys to the buffer.
	buflen = llistxattr(full_path.data(), buf, buflen);
	MTL_CHECK_VOID(buflen != -1);
	
	/** Loop over the list of zero terminated strings with the
		attribute keys. Use the remaining buffer length to determine
		the end of the list. */
	char *key = buf;
	ByteArray ba;
	while (buflen > 0)
	{
		/// Determine length of the value.
		isize vallen = lgetxattr(full_path.constData(), key, NULL, 0);
		if (vallen <= 0)
			break;
		
		ba.alloc(vallen);
		vallen = lgetxattr(full_path.constData(), key, ba.data(), ba.size());
		if (vallen == -1)
		{
			mtl_status(errno);
			break;
		}
		
		ext_attrs.insert(key, ba);
		{
			// mtl_info("Ext attr: \"%s\": \"%s\" (%s)", key, qPrintable(ba.toString()), qPrintable(file.name()));
		}
		
		/// Forward to next attribute key.
		cisize keylen = strlen(key) + 1;
		buflen -= keylen;
		key += keylen;
	}
}

bool ReloadMeta(io::File &file, struct statx &stx, const QProcessEnvironment &env,
	const PrintErrors pe, QString *dir_path)
{
	QString full_path_str;
	if (dir_path)
	{
		full_path_str = (*dir_path + file.name());
	} else {
		full_path_str = file.build_full_path();
	}
	
	QByteArray full_path = full_path_str.toLocal8Bit();
	cauto flags = AT_SYMLINK_NOFOLLOW;
	cauto fields = STATX_ALL;
	if (statx(0, full_path.data(), flags, fields, &stx) != 0)
	{
		if (pe == PrintErrors::Yes)
			mtl_warn("statx(): %s: \"%s\"", strerror(errno), full_path.data());
		return false;
	}
	
	FillInStx(file, stx, nullptr);
	file.DeleteMediaPreview();
	ReadXAttrs(file, full_path);
	
	if (file.is_symlink())
	{
		auto *target = new LinkTarget();
		ReadLink(full_path.data(), *target,
			(dir_path != nullptr) ? *dir_path : file.dir_path(Lock::Yes));
		delete file.link_target();
		file.link_target(target);
	}
	
	auto &cache = file.cache();
	if (file.is_regular() && (cache.possible_categories != nullptr)
		&& cache.ext == DesktopExt)
	{
		DesktopFile *df = DesktopFile::FromPath(full_path_str,
			*cache.possible_categories, env);
		if (cache.desktop_file != nullptr)
			delete cache.desktop_file;
		
		cache.desktop_file = df;
	}
	
	return true;
}

void RemoveEFA(QStringView full_path, QVector<QString> names, const PrintErrors pe)
{
	auto file_path_ba = full_path.toLocal8Bit();
	
	for (const auto &name: names)
	{
		auto name_ba = name.toLocal8Bit();
		cbool ok = lremovexattr(file_path_ba.data(), name_ba.data()) == 0;
		
		// if (ok) {
		// 	mtl_info("Removed %s for %s", name_ba.data(), file_path_ba.data());
		// }
		
		if (!ok && (pe == PrintErrors::Yes))
			mtl_warn("lremovexattr on %s: %s", name_ba.data(), strerror(errno));
	}
}

int Rename(QStringView old_path, QStringView new_path)
{
	auto old_ba = old_path.toLocal8Bit();
	auto new_ba = new_path.toLocal8Bit();
	
	return (::rename(old_ba.data(), new_ba.data()) == 0) ? 0 : errno;
}

bool SameFiles(const QString &path1, const QString &path2, int *ret_error)
{
	struct statx stx;
	auto ba = path1.toLocal8Bit();
	const auto flags = AT_SYMLINK_NOFOLLOW;
	const auto fields = STATX_INO;
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0)
	{
		if (ret_error != nullptr)
			*ret_error = errno;
		return false;
	}
	
	const auto id1 = DiskFileId::FromStx(stx);
	ba = path2.toLocal8Bit();
	
	if (statx(0, ba.data(), flags, fields, &stx) != 0)
	{
		if (ret_error != nullptr)
			*ret_error = errno;
		return false;
	}
	
	if (ret_error != nullptr)
		*ret_error = 0;
	
	const auto id2 = DiskFileId::FromStx(stx);
	
	return id1 == id2;
}

bool SaveThumbnailToDisk(const SaveThumbnail &item, ZSTD_CCtx *compress_ctx,
	cbool ok_to_store_thumbnails_in_ext_attrs)
{
	ByteArray ba;
	ba.MakeSure(thumbnail::HeaderSize, ExactSize::Yes);
	ba.add_i16(thumbnail::AbiVersion);
	ba.add_i16(item.thmb.width());
	ba.add_i16(item.thmb.height());
	cu16 bpl = item.thmb.bytesPerLine();
	ba.add_u16(bpl);
	ba.add_i32(item.orig_img_w);
	ba.add_i32(item.orig_img_h);
	ba.add_i32(static_cast<i32>(item.thmb.format()));
	
	const char *src_buf = (const char*)item.thmb.constBits();
	ci64 src_size = item.thmb.sizeInBytes();
	ci64 dst_size = ZSTD_compressBound(src_size);
	char *dst_buf = (char*)malloc(dst_size);
	ci64 compressed_size = ZSTD_compressCCtx(compress_ctx,
		dst_buf, dst_size, src_buf, src_size, 1);
	ba.add(dst_buf, compressed_size, ExactSize::Yes);
	free(dst_buf);
	
	if (ok_to_store_thumbnails_in_ext_attrs && (item.dir == TempDir::No))
	{
		if (io::SetEFA(item.full_path, media::XAttrThumbnail, ba, PrintErrors::No))
			return true;
	}
	
	QString file_path = io::BuildTempPathFromID(item.id);
	io::SaveFile save_file(file_path);
	if (!io::WriteToFile(save_file.GetPathToWorkWith(), ba.data(), ba.size()))
	{
		save_file.CommitCancelled();
		mtl_printq(file_path);
		mtl_printq2("File path to work with: ", save_file.GetPathToWorkWith());
		return false;
	}
	
	return save_file.Commit();
}

bool sd_nvme(const QString &name)
{
	static const QString sd = QLatin1String("sd");
	static const QString nvme = QLatin1String("nvme");
	return name.startsWith(sd) || name.startsWith(nvme);
}

bool valid_dev_path(const QString &name)
{
	static const QString sd = QLatin1String("/dev/sd");
	static const QString nvme = QLatin1String("/dev/nvme");
	return name.startsWith(sd) || name.startsWith(nvme);
}

bool SetEFA(const QString &full_path, const QString &xattr_name,
	const ByteArray &ba, const PrintErrors pe)
{
	auto file_path_ba = full_path.toLocal8Bit();
	auto xattr_name_ba = xattr_name.toLocal8Bit();
	cbool ok = lsetxattr(file_path_ba.data(), xattr_name_ba.data(),
		ba.data(), ba.size(), 0) == 0;
	
	if (!ok && pe == PrintErrors::Yes)
	{
		mtl_warn("lsetxattr \"%s\": \"%s\", on file: \"%s\"", xattr_name_ba.data(),
			strerror(errno), file_path_ba.data());
	}
	
	return ok;
}

QString SizeToString(ci64 sz, const StringLength len)
{
	float rounded;
	QString type;
	if (sz >= io::TiB) {
		rounded = sz / io::TiB;
		type = (len == StringLength::Short) ? QLatin1String("T") : QLatin1String(" TiB");
	}
	else if (sz >= io::GiB) {
		rounded = sz / io::GiB;
		type = (len == StringLength::Short) ? QLatin1String("G") : QLatin1String(" GiB");
	} else if (sz >= io::MiB) {
		rounded = sz / io::MiB;
		type = (len == StringLength::Short) ? QLatin1String("M") : QLatin1String(" MiB");
	} else if (sz >= io::KiB) {
		rounded = sz / io::KiB;
		type = (len == StringLength::Short) ? QLatin1String("K") : QLatin1String(" KiB");
	} else {
		rounded = sz;
		type = (len == StringLength::Short) ? QLatin1String("B") : QLatin1String(" bytes");
	}
	
	return io::FloatToString(rounded, 1) + type;
}

bool SortFiles(io::File *a, io::File *b) 
{
/** Note: this function MUST be implemented with strict weak ordering
  otherwise it randomly crashes (because of undefined behavior),
  more info here:
 https://stackoverflow.com/questions/979759/operator-and-strict-weak-ordering/981299#981299 */
	
	if (!a->files())
		mtl_warn("a->files is null on %s", qPrintable(a->name()));
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
	
	cbool by_created = (order.column == gui::Column::TimeCreated);
	if (by_created || (order.column == gui::Column::TimeModified))
	{
		auto &tc = by_created ? a->time_created() : a->time_modified();
		auto &tc2 = by_created ? b->time_created() : b->time_modified();
		
		if (tc.tv_sec != tc2.tv_sec) {
			cbool less_sec = (tc.tv_sec < tc2.tv_sec);
			return order.ascending ? less_sec : !less_sec;
		}
		
		if (tc.tv_nsec == tc2.tv_nsec)
			return false;
		
		cbool less_nsec = tc.tv_nsec < tc2.tv_nsec;
		return order.ascending ? less_nsec : !less_nsec;
	}
	
	if (order.column == gui::Column::Size) {
		if (a->size() == b->size()) {
			int n = CompareStrings(a->name_lower(), b->name_lower());
			bool result = n >= 0 ? false : true;
			return order.ascending ? result : !result;
		}
		cbool less_size = a->size() < b->size();
		return order.ascending ? less_size : !less_size;
	}
	
	if (order.column == gui::Column::Icon) {
		// order by file type..
		if (a->type() != b->type()) {
			cbool less_type = a->type() < b->type();
			return order.ascending ? less_type : !less_type;
		}
		// next, order by extension:
		if (a->cache().ext == b->cache().ext) {
			int n = CompareStrings(a->name_lower(), b->name_lower());
			bool result = n >= 0 ? false : true;
			return order.ascending ? result : !result;
		}
		
		cbool less_ext = a->cache().ext < b->cache().ext;
		return order.ascending ? less_ext : !less_ext;
	}
	
	mtl_trace();
	return false;
}

QString thread_id_short(const pthread_t &th)
{
	ci64 n = static_cast<i64>(th);
	return QString::number(n, 36);
}

struct timespec timespec_diff(const timespec &start, const timespec &stop)
{
	struct timespec result;
	if ((stop.tv_nsec - start.tv_nsec) < 0) {
		result.tv_sec = stop.tv_sec - start.tv_sec - 1;
		result.tv_nsec = stop.tv_nsec - start.tv_nsec + 1000000000L;
	} else {
		result.tv_sec = stop.tv_sec - start.tv_sec;
		result.tv_nsec = stop.tv_nsec - start.tv_nsec;
	}
	
	return result;
}

timespec timespec_now() {
	timespec now = {};
	
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) != 0)
	{
		mtl_trace("%s", strerror(errno));
	}
	
	return now;
}

isize TryReadFile(const QString &full_path, char *buf, ci64 how_much,
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
	
	cint fd = open(ba.data(), O_RDONLY);
	if (fd == -1)
		return -1;
	
	cisize num_bytes = read(fd, buf, how_much);
	close(fd);
	return num_bytes;
}

bool WriteToFile(const QString &full_path, const char *data, ci64 size,
	const PostWrite post_write, mode_t *custom_mode)
{
	auto path = full_path.toLocal8Bit();
	cint fd = open(path.data(), O_LARGEFILE | O_WRONLY | O_CREAT | O_TRUNC,
		(custom_mode == nullptr) ? io::FilePermissions : *custom_mode);
	if (fd == -1)
	{
		mtl_info("%s: %s", path.data(), strerror(errno));
		return false;
	}
	
	AutoCloseFd fd_(fd);
	i64 written = 0;
	i64 ret;
	
	while (written < size) {
		// ssize_t write(int fd, const void *buf, size_t count);
		ret = write(fd, data + written, size - written);
		
		if (ret == -1) {
			if (errno == EAGAIN)
				continue;
			mtl_status(errno);
			mtl_info("%s", path.data());
			return false;
		}
		
		written += ret;
	}
	
	switch (post_write) {
	case PostWrite::DoNothing: break;
	case PostWrite::FSync: fsync(fd); break;
	case PostWrite::FDataSync: fdatasync(fd); break;
	}
	
	return true;
}

} // cornus::io::



#pragma once

#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <pthread.h>
#include <QVector>
#include <QString>

#include "../gui/decl.hxx"

namespace cornus::io {
	class File;
	class Task;
	
	enum class ActUponAnswer: i8 {
		None = 0,
		Skip,
		Retry,
		Abort,
	};
	
	enum class FileExistsAnswer: i8 {
		None = 0,
		Overwrite,
		OverwriteAll,
		Skip,
		SkipAll,
		Abort
	};
	
	enum class WriteFailedAnswer: i8 {
		None = 0,
		Retry,
		Skip,
		SkipAll,
		Abort,
	};
	
	enum class TaskState: u16 {
		None = 1u << 0,
		Continue = 1u << 1,
		Pause = 1u << 2,
		Abort = 1u << 3,
		Error = 1u << 4,
		Finished = 1u << 5,
		AwatingAnswer = 1u << 6,
		Answered = 1u << 7,
	};
	
	inline TaskState operator | (TaskState a, TaskState b) {
		return static_cast<TaskState>(static_cast<u16>(a) | static_cast<u16>(b));
	}
	
	inline bool operator & (TaskState a, TaskState b) {
		return (static_cast<u16>(a) & static_cast<u16>(b));
	}
	
	enum class FileBits: u8 {
		Empty = 0,
		Selected = 1u << 0,
		ActionCut = 1u << 1,
		ActionCopy = 1u << 2,
		ActionPaste = 1u << 3,
		ActionLink = 1u << 4,
	};
	
	inline FileBits operator | (FileBits a, FileBits b) {
		return static_cast<FileBits>(static_cast<u8>(a) | static_cast<u8>(b));
	}
	
	inline FileBits& operator |= (FileBits &a, const FileBits b) {
		a = a | b;
		return a;
	}
	
	inline FileBits operator ~ (FileBits a) {
		return static_cast<FileBits>(~(static_cast<u8>(a)));
	}
	
	inline FileBits operator & (FileBits a, FileBits b) {
		return static_cast<FileBits>((static_cast<u8>(a) & static_cast<u8>(b)));
	}
	
	inline FileBits& operator &= (FileBits &a, const FileBits b) {
		a = a & b;
		return a;
	}
	
	enum ListOptions: u8 {
		HiddenFiles = 1u << 0,
	};
	
	typedef bool (*FilterFunc)(const QString &dir_path, const QString &name);
	
	const f64 KiB = 1024;
	const f64 MiB = KiB * 1024;
	const f64 GiB = MiB * 1024;
	const f64 TiB = GiB * 1024;
	
	static const mode_t DirPermissions = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
	static const mode_t FilePermissions = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
	
	struct FileID {
		u64 inode_number;
		u32 dev_major; // ID of device containing file
		u32 dev_minor;
		
		inline static FileID New(const struct stat &st) {
			return io::FileID {
				.inode_number = st.st_ino,
				.dev_major = gnu_dev_major(st.st_dev),
				.dev_minor = gnu_dev_minor(st.st_dev),
			};
		}
		
		inline static FileID NewStx(const struct statx &stx) {
			return io::FileID {
				.inode_number = stx.stx_ino,
				.dev_major = stx.stx_dev_major,
				.dev_minor = stx.stx_dev_minor,
			};
		}
		
		inline bool
		operator == (const FileID &rhs) const {
			return Equals(rhs);
		}
		
		inline bool
		Equals(const FileID &rhs) const {
			return inode_number == rhs.inode_number &&
				dev_major == rhs.dev_major && dev_minor == rhs.dev_minor;
		}
	};
	
	struct SortingOrder {
		SortingOrder() {}
		SortingOrder(gui::Column c, bool asc): column(c), ascending(asc) {}
		SortingOrder(const SortingOrder &rhs) {
			column = rhs.column;
			ascending = rhs.ascending;
		}
		gui::Column column = gui::Column::FileName;
		bool ascending = true;
	};
	
	enum class Err : u8 {
		Ok = 0,
		Access, // permission denied
		Perm, // operation not permitted
		IO, // I/O error
		CircularSymlink,
		Other
	};
	
	enum class FileType : u8 {
		Unknown = 0,
		Regular,
		Dir,
		Symlink,
		Socket,
		Pipe,
		BlockDevice,
		CharDevice
	};
	
	struct LinkTarget {
		QString path;
		QVector<FileID> chain_ids_; // for detecting circular symlinks
		QVector<QString> chain_paths_;
		mode_t mode = 0;
		FileType type = FileType::Unknown;
		i8 cycles = 1; // @cycles is set negative when circular symlink detected.
		static const i8 MaxCycles = 6;
		
		LinkTarget* Clone() {
			LinkTarget *p = new LinkTarget();
			p->path = path;
			p->chain_ids_ = chain_ids_;
			p->chain_paths_ = chain_paths_;
			p->type = type;
			p->cycles = cycles;
			return p;
		}
	};
}


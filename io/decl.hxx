#pragma once

#include <sys/sysmacros.h>
#include <sys/stat.h>

namespace cornus::io {
	class File;
	
	struct FileID {
		u64 inode_number;
		u32 dev_major; // ID of device containing file
		u32 dev_minor;
		
		inline static FileID New(const struct stat &st) {
			return io::FileID {
				.inode_number = st.st_ino,
				.dev_major = major(st.st_dev),
				.dev_minor = minor(st.st_dev),
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
}

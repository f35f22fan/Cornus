#pragma once

#include <QString>
#include <QHash>

#include "../io/decl.hxx"

namespace cornus::misc {

struct Thumbnail {
	struct io::DiskFileId id = {};
	struct statx_timestamp date = {};
	u32 time_added = 0; // minutes, not seconds since Unix Epoch
	u8 value = 0;
};

inline bool operator==(const Thumbnail &a, const Thumbnail &b)
{
	return a.id == b.id && a.date.tv_sec == b.date.tv_sec && a.date.tv_nsec == b.date.tv_nsec;
}

inline size_t qHash(const Thumbnail &key, size_t seed)
{
	return qHashMulti(seed, key.id.inode_number, key.id.dev_major, key.id.dev_minor);
}

class Blacklist {
public:
	Blacklist();
	~Blacklist();
	
	void Add(const io::DiskFileId &id, const statx_timestamp &date, u8 value, cu32 *added = 0);
	
	
	void Load();
	bool Save();
	
	bool ContainsThumbnail(io::File *file);
	void BlockThumbnail(io::File *file);
	bool AllowThumbnail(io::File *file);

private:
	QString filePath();
	
	void ToBlob(ByteArray &buf);
	
	QHash<io::DiskFileId, misc::Thumbnail> hash_;
	
	bool modified_ = false;
};

}

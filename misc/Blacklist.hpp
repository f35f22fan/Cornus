#pragma once

#include <QString>
#include <QHash>

#include "../decl.hxx"
#include "../io/decl.hxx"

namespace cornus::misc {

struct Thumbnail {
	struct io::DiskFileId id = {};
	struct statx_timestamp date = {};
	u32 time_added = 0; // minutes, not seconds since Unix Epoch
	Efa value = Efa::None;
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
	
	Efa Add(const io::DiskFileId &id, const statx_timestamp &date, Efa value, cu32 *added = 0);
	
	
	void Load();
	bool Save();
	
	bool IsAllowed(io::File *file, Efa efa);
	Efa GetStatus(io::File *file);
	Efa Block(io::File *file, Efa efa);
	Efa Allow(io::File *file, Efa allowed);

private:
	QString filePath();
	
	void ToBlob(ByteArray &buf);
	
	QHash<io::DiskFileId, misc::Thumbnail> hash_;
	
	bool modified_ = false;
};

}

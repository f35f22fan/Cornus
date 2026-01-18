#include "Blacklist.hpp"
#include "../ByteArray.hpp"
#include "../prefs.hh"
#include "../io/File.hpp"

#include <time.h>

namespace cornus::misc {

Blacklist::Blacklist() {}

Blacklist::~Blacklist() {
	if (modified_) {
		Save();
	}
}

Efa Blacklist::Add(const io::DiskFileId &id, const struct statx_timestamp &date, Efa value, cu32 *added)
{
	Thumbnail t = hash_.value(id);
	const Efa what_changed = value & ~t.value;
	// mtl_info("value: %d, t.value: %d, what_changed: %d", (int)value, (int)t.value, (int)what_changed);
	if (!t.id.empty()) {
		t.value |= value;
		hash_.insert(id, t);
		return what_changed;
	}
	
	t.id = id;
	t.date = date;
	t.value = value;
	if (added) {
		t.time_added = *added;
	} else {
		t.time_added = u32(time(NULL) / 60);
		modified_ = true;
		// mtl_info("Added disk id %lu, %u, %u", id.inode_number, id.dev_major, id.dev_minor);
	}

	hash_.insert(id, t);
	
	return what_changed;
}

Efa Blacklist::Allow(io::File *file, Efa allowed) {
	// returns what changed for the file
	Thumbnail found = hash_.value(file->id());
	if (found.id.empty()) {
		return Efa::None;
	}
	
	modified_ = true;
	Efa what_changed = allowed & ~found.value;
	found.value &= ~allowed;
	if (found.value == Efa::None) {
		hash_.remove(file->id());
	} else {
		hash_.insert(file->id(), found);
	}
	
	return what_changed;
}

Efa Blacklist::Block(io::File *file, Efa efa) {
	// returns what changed for the file
	return Add(file->id(), file->time_created(), efa);
}

Efa Blacklist::GetStatus(io::File *file) {
	if (!hash_.contains(file->id())) {
		return Efa::None;
	}
	
	const misc::Thumbnail th = hash_.value(file->id());
	
	cauto b = file->time_created();
	cbool ret = th.date.tv_sec == b.tv_sec && th.date.tv_nsec == b.tv_nsec;
	
	if (!ret) {
		modified_ = true;
		hash_.remove(file->id());
		return Efa::None;
	}
	
	return th.value;
}

QString Blacklist::filePath() {
	QString full_path = prefs::QueryAppConfigPath();
	if (!full_path.endsWith('/'))
		full_path.append('/');
	full_path.append("blacklist.bin");
	return full_path;
}

bool Blacklist::IsAllowed(io::File *file, Efa efa)
{
	Efa has = GetStatus(file);
	cbool ok = !EfaContains(has, efa);
	// mtl_info("efa: %d, has: %d, ok: %d", efa, has, ok);
	return ok;
}

void Blacklist::Load() {
	hash_.clear();
	QString path = filePath();
	ByteArray buf;
	io::ReadParams params = {};
	params.can_rely = CanRelyOnStatxSize::Yes;
	params.print_errors = PrintErrors::No;
	mtl_check_void(io::ReadFile(path, buf, params));
	io::DiskFileId id;
	struct statx_timestamp date;
	while (buf.has_more()) {
		id.inode_number = buf.next_u64();
		id.dev_major = buf.next_u32();
		id.dev_minor = buf.next_u32();
		date.tv_sec = buf.next_i64();
		date.tv_nsec = buf.next_u32();
		cu32 added = buf.next_u32();
		Efa value = (Efa)buf.next_u8();
		Add(id, date, value, &added);
	}
}

bool Blacklist::Save() {
	// returns true on success
	if (!modified_) {
		return true;
	}
	QString path = filePath();
	ByteArray buf;
	ToBlob(buf);
	
	modified_ = false;
	
	return io::WriteToFile(path, buf.data(), buf.size());
}

void Blacklist::ToBlob(ByteArray &buf)
{
	for (auto it = hash_.cbegin(); it != hash_.cend(); ++it) {
		const Thumbnail th = it.value();
		buf.add_u64(th.id.inode_number);
		buf.add_u32(th.id.dev_major);
		buf.add_u32(th.id.dev_minor);
		buf.add_i64(th.date.tv_sec);
		buf.add_u32(th.date.tv_nsec);
		buf.add_u32(th.time_added);
		buf.add_u8((EfaType)th.value);
	}
}

}

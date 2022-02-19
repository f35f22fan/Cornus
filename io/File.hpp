#pragma once

#include "../ByteArray.hpp"
#include "../gui/decl.hxx"
#include "../err.hpp"
#include "../media.hxx"
#include "io.hh"
#include "../str.hxx"
#include "../thumbnail.hh"

#include <QHash>
#include <QIcon>
#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QMimeDatabase>

namespace cornus::io {

struct FileCache {
	QString mime;
	QStringRef ext = QStringRef();
	QIcon *icon = nullptr;
	DesktopFile *desktop_file = nullptr;
	const QHash<QString, Category> *possible_categories = nullptr;
	media::ShortData *short_data = nullptr;
	Thumbnail *thumbnail = nullptr;
	bool tried_loading_thumbnail = false;
};

class File {
public:
	File(Files *files);
	File(const QString &dir_path);
	virtual ~File();
	
	static File* NewTextFile(const QString &dir_path, const QString &name);
	static File* NewFolder(const QString &dir_path, const QString &name);
	int Delete(); // returns 0 on success
	
	File* Clone() const;
	void CopyBits(File *rhs) {
		bits_ = rhs->bits_;
	}
	
	QString build_full_path() const;
	FileCache& cache() { return cache_; }
	void ClearCache();
	bool has(const io::FileBits bits) { return (bits_ & bits) != FileBits::Empty; }
	bool has_exec_bit() const;
	bool has_link_target() const { return is_symlink() && link_target_ != nullptr; }
	bool can_have_xattr() const { return is_regular() ||
		is_symlink() || is_dir(); }
	QHash<QString, ByteArray>& ext_attrs() { return ext_attrs_; }
	bool has_ext_attrs() const { return ext_attrs_.size() > 0; }
	bool has_media_attrs() const { return ext_attrs_.contains(media::XAttrName);}
	ByteArray media_attrs() const { return ext_attrs_.value(media::XAttrName); }
	bool has_last_watched_attr() const { return ext_attrs_.contains(media::XAttrLastWatched); }
	bool has_thumbnail_attr() const { return ext_attrs_.contains(media::XAttrThumbnail); }
	ByteArray thumbnail_attrs() const { return ext_attrs_.value(media::XAttrThumbnail); }
	ByteArray& thumbnail_attrs_ref() { return ext_attrs_[media::XAttrThumbnail]; }
	bool is_desktop_file() const { return is_regular() &&
		cache_.ext == str::Desktop; }
	bool IsThumbnailMarkedFailed();
	void MarkThumbnailFailed();
	media::ShortData* media_attrs_decoded();
	void ClearXAttrs();
	void ReadLinkTarget();
	
	bool is_dir() const { return type_ == FileType::Dir; }
	bool is_link_to_dir() const { return is_symlink() &&
		(link_target_ != nullptr) &&
		(link_target_->type == FileType::Dir); }
	bool is_dir_or_so() const { return is_dir() || is_link_to_dir(); }
	bool is_regular() const { return type_ == FileType::Regular; }
	bool is_symlink() const { return type_ == FileType::Symlink; }
	bool is_pipe() const { return type_ == FileType::Pipe; }
	bool is_socket() const { return type_ == FileType::Socket; }
	bool is_bloc_device() const { return type_ == FileType::BlockDevice; }
	bool is_char_device() const { return type_ == FileType::CharDevice; }
	inline void toggle_flag(const FileBits flag, const bool do_add) {
		if (do_add)
			bits_ |= flag;
		else {
			bits_ &= ~flag;
		}
	}
	
	const FileBits AllActions = FileBits::ActionCopy | FileBits::ActionCut |
		FileBits::ActionPaste | FileBits::PasteLink;
	
	bool clear_all_actions_if_needed() {
		/// returns true if anything was cleared for the upstream
		/// to be able to decide whether it needs to be repainted
		if ((bits_ & AllActions) != FileBits::Empty) {
			toggle_flag(AllActions, false);
			return true;
		}
		return false;
	}
	
	bool action_copy() const { return (bits_ & FileBits::ActionCopy) != FileBits::Empty; }
	void action_copy(const bool add) {
		toggle_flag(FileBits::ActionCopy, add);
		toggle_flag(AllActions & ~FileBits::ActionCopy, false);
	}
	
	bool action_cut() const { return (bits_ & FileBits::ActionCut) != FileBits::Empty; }
	void action_cut(const bool add) {
		toggle_flag(FileBits::ActionCut, add);
		toggle_flag(AllActions & ~FileBits::ActionCut, false);
	}
	
	bool action_paste_link() const { return (bits_ & FileBits::PasteLink) != FileBits::Empty; }
	void action_paste_link(const bool add) {
		toggle_flag(FileBits::PasteLink, add);
		toggle_flag(AllActions & ~FileBits::PasteLink, false);
	}
	
	bool action_paste() const { return (bits_ & FileBits::ActionPaste) != FileBits::Empty; }
	void action_paste(const bool add) {
		toggle_flag(FileBits::ActionPaste, add);
		toggle_flag(AllActions & ~FileBits::ActionPaste, false);
	}
	
	bool is_selected() const { return (bits_ & FileBits::Selected) != FileBits::Empty; }
	void set_selected(const bool yes) { toggle_flag(FileBits::Selected, yes); }
	cornus::Selected selected() const {
		return is_selected() ? Selected::Yes : Selected::No;
	}
	void selected(const Selected flag) { set_selected(flag == Selected::Yes); }
	void toggle_selected() { set_selected(!is_selected()); }
	
	bool selected_by_search() const { return (bits_ & FileBits::SelectedBySearch) != FileBits::Empty; }
	void selected_by_search(const bool add) { toggle_flag(FileBits::SelectedBySearch, add); }
	
	bool selected_by_search_active() const { return (bits_ & FileBits::SelectedBySearchActive) != FileBits::Empty; }
	void selected_by_search_active(const bool add) { toggle_flag(FileBits::SelectedBySearchActive, add); }
	
	LinkTarget *link_target() const { return link_target_; }
	void link_target(LinkTarget *p) { link_target_ = p; }
	
	void mode(mode_t m) { mode_ = m; }
	mode_t mode() const { return mode_; }
	
	const QString& name() const { return name_.orig; }
	const QString& name_lower() const { return name_.lower; }
	void name(const QString &s);
	
	void dir_path(const QString &s) { dp_ = s; }
	
	const QString& dir_path() const;
	
	io::Files* files() const { return files_; }
	void files(io::Files *ptr) { files_ = ptr; dp_.clear(); }
	
	void id(const DiskFileId d) { id_ = d; }
	const DiskFileId& id() const { return id_; }
	u64 id_num() const { return id_.inode_number; }
	
	QString SizeToString() const;
	
	i64 size() const { return size_; }
	void size(const i64 n) { size_ = n; }
	
	bool ShouldTryLoadingThumbnail();
	
	Thumbnail* thumbnail() const { return cache_.thumbnail; }
	void thumbnail(Thumbnail *p)
	{
		if (cache_.thumbnail)
			delete cache_.thumbnail;
		cache_.thumbnail = p;
	}
	
	const struct statx_timestamp&
	time_created() const { return time_created_; }
	void time_created(const struct statx_timestamp &t) { time_created_ = t; }
	
	const struct statx_timestamp&
	time_modified() const { return time_modified_; }
	void time_modified(const struct statx_timestamp &t) { time_modified_ = t; }
	
	void type(const FileType t) { type_ = t; }
	FileType type() const { return type_; }
	
	void CountDirFiles1Level();
	int dir_file_count_1_level() const { return dir_file_count_1_level_; }
	
private:
	NO_ASSIGN_COPY_MOVE(File);
	
	void ReadExtension();
	
	struct Name {
		QString orig;
		QString lower;
	} name_ = {};
	
	LinkTarget *link_target_ = nullptr;
	FileCache cache_ = {};
	io::Files *files_ = nullptr;
	QString dp_;
	i64 size_ = -1;
	QHash<QString, ByteArray> ext_attrs_;
	DiskFileId id_ = {};
	struct statx_timestamp time_created_ = {};
	struct statx_timestamp time_modified_ = {};
	mode_t mode_;
	int dir_file_count_1_level_ = -1;
	io::FileBits bits_ = FileBits::Empty;
	FileType type_ = FileType::Unknown;
	
	friend class cornus::gui::TableModel;
};

}

Q_DECLARE_METATYPE(cornus::io::File*);

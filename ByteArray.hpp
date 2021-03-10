#pragma once

#include "io/socket.hh"
#include "types.hxx"

#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QString>
#include <vector>

namespace cornus {

class ByteArray {
public:
	ByteArray();
	virtual ~ByteArray();
	
	void alloc(const usize exact_size);
	
	void add(const char *n, const usize size);
	void add_i8(const i8 n);
	void add_u8(const u8 n);
	void add_i16(const i16 n);
	void add_u16(const u16 n);
	void add_i32(const i32 n);
	void add_u32(const u32 n);
	void add_i64(const i64 n);
	void add_u64(const u64 n);
	void add_f32(const float n);
	void add_f64(const double n);
	void add_string(const QString &s);
	void Clear();
	char *data() const { return data_; }
	const char *constData() const { return data_; }
	
	bool has_more() const { return at_ < size_; }
	bool is_empty() const { return size_ == 0; }
	void next(char *p, const usize sz);
	i8 next_i8();
	u8 next_u8();
	i16 next_i16();
	u16 next_u16();
	i32 next_i32();
	u32 next_u32();
	i64 next_i64();
	u64 next_u64();
	float next_f32();
	double next_f64();
	QString next_string();
	
	usize alloc_size() const { return size_; }
	usize at() const { return at_; }
	usize size() const { return size_; }
	void size(usize n) { size_ = n; } // called from inside io::ReadFile(..);
	void make_sure(const usize more_bytes, const bool exact_size = false);
	inline void to(usize n) { at_ = n; }
	bool Receive(int fd, bool close_socket = true);
	bool Send(int fd, bool close_socket = true) const;
	void set_msg_id(const io::socket::MsgBits msg_id);
	QString toString() const { return QString::fromLocal8Bit(data_, size_); }
private:
	
	char *data_ = nullptr;
	usize size_ = 0;
	usize heap_size_ = 0;
	usize at_ = 0;
};

}
Q_DECLARE_METATYPE(cornus::ByteArray*);

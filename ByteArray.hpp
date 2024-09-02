#pragma once

#include "io/socket.hh"
#include "io/io.hh"
#include "types.hxx"

#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QString>
#include <vector>

namespace cornus {

enum class CloseSocket: i8 {
	Yes,
	No,
};

class ByteArray: public QObject {
	Q_OBJECT
public:
	ByteArray();
	explicit ByteArray(i64 num);
	explicit ByteArray(u64 num);
	ByteArray(const ByteArray &rhs);
	ByteArray& operator = (const ByteArray &rhs);
	bool operator == (const ByteArray &rhs);
	ByteArray* CloneFromHere();
	virtual ~ByteArray();
	
	void alloc(const isize n);
	
	void add(const ByteArray *ba, const From from);
	void add(const char *p, const isize size, const ExactSize es = ExactSize::No);
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
	isize at() const { return at_; }
	void Clear();
	char *data() const { return data_; }
	const char *constData() const { return data_; }
	
	bool has_more(const isize n) const { return at_ + n <= size_; }
	bool has_more() const { return at_ < size_; }
	bool is_empty() const { return size_ == 0; }
	void next(char *p, const isize sz);
	i8 next_i8();
	u8 next_u8();
	i16 next_i16();
	u16 next_u16();
	i32 next_i32();
	u32 next_u32();
	i64 next_i64();
	u64 next_u64();
	f32 next_f32();
	f64 next_f64();
	QString next_string();
	
	isize alloc_size() const { return size_; }
	
	isize size() const { return size_; }
	isize heap_size() const { return heap_size_; }
	void size(isize n) { size_ = n; } // called from inside io::ReadFile(..);
	void MakeSure(isize more_bytes, const ExactSize es = ExactSize::No);
	inline void to(isize n) { at_ = n; }
	bool Receive(cint fd, const CloseSocket cs = CloseSocket::Yes);
	bool Send(int fd, const CloseSocket cs = CloseSocket::Yes) const;
	void set_msg_id(const cornus::io::Message msg_id);
	QString toString() const { return QString::fromLocal8Bit(data_, size_); }

private:
///	NO_ASSIGN_COPY_MOVE(ByteArray);
	
	isize size_ = 0;
	isize heap_size_ = 0;
	isize at_ = 0;
	char *data_ = nullptr;
};

}
Q_DECLARE_METATYPE(cornus::ByteArray*);

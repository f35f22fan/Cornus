#pragma once

#include "io/socket.hh"
#include "io/io.hh"
#include "types.hxx"

#include <QMetaType> /// Q_DECLARE_METATYPE()
#include <QString>
#include <vector>

namespace cornus {

enum class CloseSocket: i1 {
	Yes,
	No,
};

class ByteArray {
public:
	ByteArray();
	ByteArray(const ByteArray &rhs);
	ByteArray& operator = (const ByteArray &rhs);
	bool operator == (const ByteArray &rhs);
	ByteArray* CloneFromHere();
	virtual ~ByteArray();
	
	void alloc(const isize n);
	
	void add(const ByteArray *ba, const From from);
	void add(const char *p, const isize size, const ExactSize es = ExactSize::No);
	void add_i1(const i1 n);
	void add_u1(const u1 n);
	void add_i2(const i2 n);
	void add_u2(const u2 n);
	void add_i4(const i4 n);
	void add_u4(const u4 n);
	void add_i8(const i8 n);
	void add_u8(const u8 n);
	void add_f4(const float n);
	void add_f8(const double n);
	void add_string(const QString &s);
	isize at() const { return at_; }
	void Clear();
	char *data() const { return data_; }
	const char *constData() const { return data_; }
	
	bool has_more(const isize n) const { return at_ + n <= size_; }
	bool has_more() const { return at_ < size_; }
	bool is_empty() const { return size_ == 0; }
	void next(char *p, const isize sz);
	i1 next_i1();
	u1 next_u1();
	i2 next_i2();
	u2 next_u2();
	i4 next_i4();
	u4 next_u4();
	i8 next_i8();
	u8 next_u8();
	f4 next_f4();
	f8 next_f8();
	QString next_string();
	
	isize alloc_size() const { return size_; }
	
	isize size() const { return size_; }
	isize heap_size() const { return heap_size_; }
	void size(isize n) { size_ = n; } // called from inside io::ReadFile(..);
	void MakeSure(isize more_bytes, const ExactSize es = ExactSize::No);
	inline void to(isize n) { at_ = n; }
	bool Receive(int fd, const CloseSocket cs = CloseSocket::Yes);
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

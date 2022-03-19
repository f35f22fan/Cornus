#include "ByteArray.hpp"

#include "err.hpp"

#include <string.h>

namespace cornus {

ByteArray::ByteArray() {}
ByteArray::ByteArray(const ByteArray &rhs)
{
	add(rhs.data(), rhs.size());
	at_ = 0;
}
ByteArray::~ByteArray()
{
	Clear();
}

ByteArray& ByteArray::operator = (const ByteArray &rhs)
{
	add(rhs.data(), rhs.size());
	at_ = 0;
	return *this;
}

bool ByteArray::operator == (const ByteArray &rhs)
{
	if (size_ != rhs.size_)
		return false;
	
	return (memcmp(data_, rhs.data(), size_) == 0);
}

void ByteArray::Clear() {
	delete[] data_;
	data_ = nullptr;
	size_ = heap_size_ = at_ = 0;
}

ByteArray* ByteArray::CloneFromHere()
{
	const i8 left = size_ - at_;
	if (left <= 0)
		return nullptr;
	
	ByteArray *ret = new ByteArray();
	ret->add(this, From::CurrentPosition);
	
	return ret;
}

void ByteArray::add(const ByteArray *ba, const From from)
{
	if (!ba)
		return; // checking is a must
	
	char *buf;
	i8 buf_len;
	if (from == From::CurrentPosition)
	{
		buf = ba->data() + ba->at();
		buf_len = ba->size() - ba->at();
	} else {
		buf = ba->data();
		buf_len = ba->size();
	}
	
	add(buf, buf_len, ExactSize::Yes);
}

void ByteArray::add(const char *p, const isize size, const ExactSize es)
{
	MakeSure(size, es);
	memcpy(data_ + at_, p, size);
	at_ += size;
	size_ += size;
}

void ByteArray::add_i1(const i1 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_u1(const u1 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_i2(const i2 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_u2(const u2 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_i4(const i4 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_u4(const u4 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_i8(const i8 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_u8(const u8 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_f4(const float n)
{
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_f8(const double n)
{
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_string(const QString &s)
{
	auto ba = s.toLocal8Bit();
	ci4 size = ba.size();
	add(reinterpret_cast<const char*>(&size), sizeof size);
	add(ba.data(), size);
}

void ByteArray::alloc(const isize n)
{
// Usage:
// ByteArray ba;
// ba.alloc(vallen);
// vallen = lgetxattr(full_path.constData(), key, ba.data(), ba.size());
	MakeSure(n, ExactSize::Yes);
	size_ = n;
}

void ByteArray::MakeSure(isize more, const ExactSize es)
{
	if (heap_size_ >= at_ + more)
		return;
	
	heap_size_ += more;
	if (es != ExactSize::Yes)
		heap_size_ *= 1.3;
	
	char *p = new char[heap_size_];
	if (data_ != nullptr)
	{
		memcpy(p, data_, size_);
		delete[] data_;
	}
	
	data_ = p;
}

void ByteArray::next(char *p, const isize sz) {
	memcpy(p, data_ + at_, sz);
	at_ += sz;
}

i1 ByteArray::next_i1() {
	i1 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u1 ByteArray::next_u1() {
	u1 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

i2 ByteArray::next_i2() {
	i2 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u2 ByteArray::next_u2() {
	u2 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

i4 ByteArray::next_i4() {
	i4 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u4 ByteArray::next_u4() {
	u4 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

i8 ByteArray::next_i8() { 
	i8 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u8 ByteArray::next_u8() {
	u8 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

f4 ByteArray::next_f4()
{
	float n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

f8 ByteArray::next_f8()
{
	double n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

QString ByteArray::next_string()
{
	ci4 size = next_i4();
	auto s = QString::fromLocal8Bit(data_ + at_, size);
	at_ += size;
	return s;
}

bool ByteArray::Receive(int fd,  const CloseSocket cs)
{
	isize total_bytes;
	if (read(fd, (char*)&total_bytes, sizeof(total_bytes)) != sizeof(total_bytes))
	{
		mtl_trace();
		return false;
	}
	
	MakeSure(total_bytes, ExactSize::Yes);
	size_ = 0;
	
	while (true)
	{
		isize count = read(fd, data_ + size_, total_bytes - size_);
		if (count == -1) {
			perror("read");
			break;
		} else if (count == 0) {
			break; // EOF
		}
		
		size_ += count;
	}
	
	at_ = 0;
	
	if (cs == CloseSocket::Yes)
	{
		int status = ::close(fd);
		if (status != 0)
			mtl_status(errno);
	}
	
	return size_ == total_bytes;
}

bool ByteArray::Send(int fd, const CloseSocket cs) const
{
	if (fd == -1)
		return false;
	isize so_far;
	{ // first send buffer size:
		so_far = write(fd, (char*)&size_, sizeof(size_));
		
		if (so_far != sizeof(size_)) {
			if (cs == CloseSocket::Yes)
				::close(fd);
			return false;
		}
	}
	
	so_far = 0;
	
	while (so_far < size_)
	{
		isize count = write(fd, data_ + so_far, size_ - so_far);
		
		if (count == -1) {
			if (errno == EAGAIN)
				continue;
			
			mtl_status(errno);
			if (cs == CloseSocket::Yes)
				::close(fd);
			
			return false;
		} else if (count == 0) { // EOF
			break;
		}
		
		so_far += count;
	}
	
	if (cs == CloseSocket::Yes)
	{
		int status = ::close(fd);
		if (status != 0)
			mtl_status(errno);
	}
	
	return so_far == size_;
}

void ByteArray::set_msg_id(const io::Message msg_type)
{
	add(reinterpret_cast<const char*>(&msg_type), sizeof msg_type);
}

}

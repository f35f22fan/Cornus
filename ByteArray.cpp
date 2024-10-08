#include "ByteArray.hpp"

#include "err.hpp"

#include <string.h>

namespace cornus {

ByteArray::ByteArray() {}
ByteArray::ByteArray(i64 num) {
	add_i64(num);
}
ByteArray::ByteArray(u64 num) {
	add_u64(num);
}
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
	const i64 left = size_ - at_;
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
	i64 buf_len;
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

void ByteArray::add_i8(const i8 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_u8(const u8 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_i16(const i16 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_u16(const u16 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_i32(const i32 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_u32(const u32 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_i64(const i64 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_u64(const u64 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_f32(const float n)
{
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_f64(const double n)
{
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::add_string(const QString &s)
{
	auto ba = s.toLocal8Bit();
	ci32 size = ba.size();
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

i16 ByteArray::next_i16() {
	i16 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u16 ByteArray::next_u16() {
	u16 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

i32 ByteArray::next_i32() {
	i32 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u32 ByteArray::next_u32() {
	u32 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

i64 ByteArray::next_i64() { 
	i64 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u64 ByteArray::next_u64() {
	u64 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

f32 ByteArray::next_f32()
{
	float n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

f64 ByteArray::next_f64()
{
	double n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

QString ByteArray::next_string()
{
	ci32 size = next_i32();
	auto s = QString::fromLocal8Bit(data_ + at_, size);
	at_ += size;
	return s;
}

bool ByteArray::Receive(cint fd,  const CloseSocket cs)
{
	i64 total_bytes;
	if (::read(fd, (char*)&total_bytes, sizeof(total_bytes)) != sizeof(total_bytes))
	{
		mtl_trace();
		return false;
	}
	
	MakeSure(total_bytes, ExactSize::Yes);
	size_ = 0;
	
	while (true)
	{
		i64 count = ::read(fd, data_ + size_, total_bytes - size_);
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
		cint status = ::close(fd);
		if (status != 0)
			mtl_status(errno);
	}
	
	return size_ == total_bytes;
}

bool ByteArray::Send(int fd, const CloseSocket cs) const
{
	if (fd == -1)
		return false;
	i64 so_far;
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
		ci64 count = write(fd, data_ + so_far, size_ - so_far);
		
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
		cint status = ::close(fd);
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

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
	ByteArray *ret = new ByteArray();
	const i64 left = size() - at();
	if (left == 0)
		return ret;
	
	ret->MakeSure(left);
	ret->add(data_ + left, left);
	
	return ret;
}

void ByteArray::add(const ByteArray *ba)
{
	if (ba)
	{
		// checking is a must
		add(ba->data(), ba->size());
	}
}

void ByteArray::add(const char *p, const isize size)
{
	MakeSure(size);
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
	i32 size = ba.size();
	add(reinterpret_cast<const char*>(&size), sizeof size);
	add(ba.data(), size);
}

void ByteArray::alloc(const isize n)
{
	MakeSure(n);
	size_ = n;
}

ByteArray ByteArray::From(const QString &s) {
	ByteArray ba;
	ba.add_string(s);
	return ba;
}

void ByteArray::MakeSure(const isize more_bytes, const ExactSize es)
{
	const isize more = (es == ExactSize::Yes) ? more_bytes : more_bytes * 3;
	const isize new_size = at_ + more;
	
	if (heap_size_ >= new_size)
		return;
	
	heap_size_ = new_size;
	char *p = new char[heap_size_];
	if (data_ == nullptr)
	{
		data_ = p;
		return;
	}
	
	memcpy(p, data_, at_);
	delete[] data_;
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

float ByteArray::next_f32()
{
	float n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

double ByteArray::next_f64()
{
	double n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

QString ByteArray::next_string()
{
	i32 size = next_i32();
	auto s = QString::fromLocal8Bit(data_ + at_, size);
	at_ += size;
	return s;
}

void ByteArray::prepend_u64(const u64 n)
{
	Prepend(reinterpret_cast<const char*>(&n), sizeof n);
}

void ByteArray::Prepend(const char *p, const isize size)
{
	MakeSure(size);
	
	char *new_heap = (char*) malloc(heap_size_);
	VOID_RET_IF(new_heap, nullptr);
	memcpy(new_heap, p, size);
	memcpy(new_heap + size, data_, size_);
	
	delete[] data_;
	data_ = new_heap;
	size_ += size;
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

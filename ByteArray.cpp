#include "ByteArray.hpp"

#include "err.hpp"

#include <string.h>

namespace cornus {

ByteArray::ByteArray() {}
ByteArray::~ByteArray()
{
	delete[] data_;
	data_ = nullptr;
}

void
ByteArray::add(const char *n, const usize size)
{
	make_sure(size);
	memcpy(data_ + at_, n, size);
	at_ += size;
	size_ += size;
}

void
ByteArray::add_i8(const i8 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_u8(const u8 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_i16(const i16 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_u16(const u16 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_i32(const i32 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_u32(const u32 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_i64(const i64 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_u64(const u64 n) {
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_f32(const float n)
{
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_f64(const double n)
{
	add(reinterpret_cast<const char*>(&n), sizeof n);
}

void
ByteArray::add_string(const QString &s)
{
	auto ba = s.toLocal8Bit();
	i32 size = ba.size();
	add(reinterpret_cast<const char*>(&size), sizeof size);
	add(ba.data(), size);
}

void
ByteArray::alloc(const usize exact_size)
{
	if (data_ != nullptr)
		mtl_trace();
	
	heap_size_ = exact_size;
	data_ = new char[exact_size];
}

void
ByteArray::make_sure(const usize more_bytes)
{
	const usize new_size = at_ + more_bytes;
	
	if (new_size > heap_size_) {
		if (data_ == nullptr) {
			heap_size_ = more_bytes;// * 64;
			data_ = new char[heap_size_];
		} else {
			heap_size_ = new_size * 1.3;
			char *p = new char[heap_size_];
			memcpy(p, data_, at_);
			delete[] data_;
			data_ = p;
		}
	}
}

void
ByteArray::next(char *p, const usize sz) {
	memcpy(p, data_ + at_, sz);
	at_ += sz;
}

i8
ByteArray::next_i8() {
	i8 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u8
ByteArray::next_u8() {
	u8 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

i16
ByteArray::next_i16() {
	i16 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u16
ByteArray::next_u16() {
	u16 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

i32
ByteArray::next_i32() {
	i32 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u32
ByteArray::next_u32() {
	u32 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

i64
ByteArray::next_i64() { 
	i64 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

u64
ByteArray::next_u64() {
	u64 n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

float
ByteArray::next_f32()
{
	float n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

double
ByteArray::next_f64()
{
	double n;
	next(reinterpret_cast<char*>(&n), sizeof n);
	return n;
}

QString
ByteArray::next_string()
{
	i32 size = next_i32();
	auto s = QString::fromLocal8Bit(data_ + at_, size);
	at_ += size;
	return s;
}

}

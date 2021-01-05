#pragma once

#include <cstdint>
#include <cstddef>
#ifdef __unix__
#include <unistd.h>
#endif

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;
typedef size_t usize;
typedef ssize_t isize;
typedef float f32;
typedef double f64;
typedef unsigned char uchar;

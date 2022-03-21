#pragma once

#include <cstdint>
#include <cstddef>
#ifdef __unix__
#include <unistd.h>
#endif

using u1 = uint8_t;
using i1 = int8_t;
using u2 = uint16_t;
using i2 = int16_t;
using u4 = uint32_t;
using i4 = int32_t;
using u8 = uint64_t;
using i8 = int64_t;
using usize = size_t;
using isize = ssize_t;
using f4 = float;
using f8 = double;
using uchar = unsigned char;

using cu1 = const u1;
using ci1 = const i1;
using cu2 = const u2;
using ci2 = const i2;
using cu4 = const u4;
using ci4 = const i4;
using cu8 = const u8;
using ci8 = const i8;
using cusize = const usize;
using cisize = const isize;
using cint = const int;
using cf4 = const f4;
using cf8 = const f8;
using cbool = const bool;
#define cauto const auto

#pragma once

#include "types.hxx"

#include <cstdio>
#include <errno.h>
#include <string.h>

#ifndef SRC_FILE_NAME
#define SRC_FILE_NAME __FILE__
#endif

#define MTL_COLOR_BLUE		"\x1B[34m"
#define MTL_COLOR_DEFAULT	"\x1B[0m"
#define MTL_COLOR_GREEN		"\x1B[32m"
#define MTL_COLOR_RED		"\e[0;91m"
#define MTL_COLOR_YELLOW    "\e[93m"
#define MTL_COLOR_MAGENTA   "\e[35m"
#define MTL_BLINK_START     "\e[5m"
#define MTL_BLINK_END       "\e[25m"
#define MTL_BOLD            "\e[1m"
#define MTL_NON_BOLD        "\e[21"
#define MTL_INVERTED		"\e[7m"
#define MTL_INVERTED_CANCEL "\e[27m"

#define mtl_info(fmt, args...) fprintf(stdout, \
	"%s[%s:%.3d %s]%s " fmt "\n", MTL_COLOR_BLUE, SRC_FILE_NAME, \
	__LINE__, __FUNCTION__, MTL_COLOR_DEFAULT, ##args)

#define mtl_infon(fmt, args...) fprintf(stdout, \
	"%s[%s:%.3d %s]%s " fmt "", MTL_COLOR_BLUE, SRC_FILE_NAME, \
	__LINE__, __FUNCTION__, MTL_COLOR_DEFAULT, ##args)

#define mtl_warn(fmt, args...) fprintf(stderr, \
	"%s[%s:%.3d %s] " fmt "%s\n", MTL_COLOR_RED, SRC_FILE_NAME, \
	__LINE__, __FUNCTION__, ##args, MTL_COLOR_DEFAULT)

#define mtl_trace(fmt, args...) fprintf(stderr, \
	"%s%s[%s:%.3d %s]%s%s " fmt "\n", MTL_BOLD, MTL_COLOR_MAGENTA, SRC_FILE_NAME, \
	__LINE__, __FUNCTION__, MTL_NON_BOLD, MTL_COLOR_DEFAULT, ##args)

#define mtl_status(status) fprintf (stderr, "%s[%s %.3d] %s%s\n", \
	MTL_COLOR_RED, SRC_FILE_NAME, \
	__LINE__, strerror(status), MTL_COLOR_DEFAULT)

#define mtl_errno() mtl_status(errno)

#define mtl_printq(s) {\
	auto __FILE____LINE__ = s.toLocal8Bit();\
	mtl_info("%s", __FILE____LINE__.data());\
}

#define mtl_printq2(msg, s) {\
	auto __FILE____LINE__ = s.toLocal8Bit();\
	mtl_info("%s\"%s\"", msg, __FILE____LINE__.data());\
}

#define mtl_tbd() {\
	mtl_trace("TBD");\
}

#define NO_ASSIGN_COPY_MOVE(TypeName)\
	TypeName(const TypeName&) = delete;\
	void operator=(const TypeName&) = delete;\
	TypeName(TypeName&&) = delete;

#define NO_MOVE(TypeName) \
	TypeName(TypeName&&) = delete;

#define RET_IF_EQUAL(x, y) {\
	if (x == y) {\
		mtl_trace();\
		return false;\
	}\
}

#define RET_IF_EQUAL_VOID(x, y) {\
	if (x == y) {\
		mtl_trace();\
		return;\
	}\
}

#define RET_IF_EQUAL_NULL(x, y) {\
	if (x == y) {\
		mtl_trace();\
		return nullptr;\
	}\
}

#define CHECK_EQUAL(x, y) {\
	if (x != y) {\
		mtl_trace();\
		return false;\
	}\
}

#define CHECK_EQUAL_VOID(x, y) {\
	if (x != y) {\
		mtl_trace();\
		return;\
	}\
}

#define CHECK_EQUAL_NULL(x, y) {\
	if (x != y) {\
		mtl_trace();\
		return nullptr;\
	}\
}

#define CHECK_TRUE(x) {\
	if (!x) {\
		mtl_trace();\
		return false;\
	}\
}

#define CHECK_TRUE_NULL(x) {\
	if (!x) {\
		mtl_trace();\
		return nullptr;\
	}\
}

#define CHECK_TRUE_VOID(x) {\
	if (!x) {\
		mtl_trace();\
		return;\
	}\
}

#define CHECK_TRUE_QSTR(x) {\
	if (!x) {\
		mtl_trace();\
		return QString();\
	}\
}

#define CHECK_PTR(x) {\
	if (x == nullptr) {\
		mtl_trace();\
		return false;\
	}\
}

#define CHECK_PTR_VOID(x) {\
	if (x == nullptr) {\
		mtl_trace();\
		return;\
	}\
}

#define CHECK_PTR_NULL(x) {\
	if (x == nullptr) {\
		mtl_trace();\
		return nullptr;\
	}\
}

#define CHECK_VK(x) {\
	if (x != VK_SUCCESS) {\
		mtl_trace();\
		return false;\
	}\
}

#define CHECK_VK_HANDLE(x) {\
	if (x != VK_SUCCESS) {\
		mtl_trace();\
		return VK_NULL_HANDLE;\
	}\
}

#define CHECK_HANDLE(x) {\
	if (x == VK_NULL_HANDLE) {\
		mtl_trace();\
		return false;\
	}\
}

#define CHECK_VK_VOID(x) {\
	if (x != VK_SUCCESS) {\
		mtl_trace();\
		return;\
	}\
}

#define CHECK_VK_CNT(x) {\
	if (x != VK_SUCCESS) {\
		mtl_trace();\
		continue;\
	}\
}

#define MTL_PTR_TO_INT(a) { (int)(i64)a }
#define MTL_INT_TO_PTR(i) { (isize*)(isize)i }

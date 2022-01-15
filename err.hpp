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
#define MTL_BOLD_START      "\e[1m"
#define MTL_BOLD_END        "\033[0m"
#define MTL_INVERTED		"\e[7m"
#define MTL_INVERTED_CANCEL "\e[27m"
#define MTL_UNDERLINE_START "\033[4m"
#define MTL_UNDERLINE_END   "\033[0m"

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
	"%s%s[%s:%.3d %s]%s%s " fmt "\n", MTL_BOLD_START, MTL_COLOR_MAGENTA, SRC_FILE_NAME, \
	__LINE__, __FUNCTION__, MTL_BOLD_END, MTL_COLOR_DEFAULT, ##args)

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

#define ret_if(lhs, rhs, ret) {\
	if ((lhs == rhs)) {\
		return ret;\
	}\
}

#define ret_if_not(lhs, rhs, ret) {\
	if ((lhs != rhs)) {\
		return ret;\
	}\
}

#define void_ret_if(lhs, rhs) {\
	if ((lhs == rhs)) {\
		return;\
	}\
}

#define void_ret_if_not(lhs, rhs) {\
	if ((lhs != rhs)) {\
		return;\
	}\
}

#define RET_IF(lhs, rhs, ret) {\
	if ((lhs == rhs)) {\
		mtl_trace();\
		return ret;\
	}\
}

#define RET_IF_NOT(lhs, rhs, ret) {\
	if ((lhs != rhs)) {\
		mtl_trace();\
		return ret;\
	}\
}

#define VOID_RET_IF(lhs, rhs) {\
	if ((lhs == rhs)) {\
		mtl_trace();\
		return;\
	}\
}

#define VOID_RET_IF_NOT(lhs, rhs) {\
	if ((lhs != rhs)) {\
		mtl_trace();\
		return;\
	}\
}

#define MTL_PTR_TO_INT(a) { (int)(i64)a }
#define MTL_INT_TO_PTR(i) { (isize*)(isize)i }

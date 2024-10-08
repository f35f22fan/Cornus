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
#define MTL_COLOR_RED		"\x1B[0;91m"
#define MTL_COLOR_YELLOW    "\x1B[93m"
#define MTL_COLOR_MAGENTA   "\x1B[35m"
#define MTL_BLINK_START     "\x1B[5m"
#define MTL_BLINK_END       "\x1B[25m"
#define MTL_BOLD_START      "\x1B[1m"
#define MTL_BOLD_END        "\x1B[0m"
#define MTL_INVERTED		"\x1B[7m"
#define MTL_INVERTED_CANCEL "\x1B[27m"
#define MTL_UNDERLINE_START "\x1B[4m"
#define MTL_UNDERLINE_END   "\x1B[0m"

#define mtl_info(fmt, args...) fprintf(stdout, \
	"%s[%s:%.3d %s]%s " fmt "\n", MTL_COLOR_BLUE, SRC_FILE_NAME, \
	__LINE__, __FUNCTION__, MTL_COLOR_DEFAULT, ##args)

#define mtl_warn(fmt, args...) fprintf(stdout, \
	"%s[%s:%.3d %s] " fmt "%s\n", MTL_COLOR_RED, SRC_FILE_NAME, \
	__LINE__, __FUNCTION__, ##args, MTL_COLOR_DEFAULT)

#define mtl_trace(fmt, args...) fprintf(stdout, \
	"%s%s[%s:%.3d %s]%s%s " fmt "\n", MTL_BOLD_START, MTL_COLOR_MAGENTA, SRC_FILE_NAME, \
	__LINE__, __FUNCTION__, MTL_BOLD_END, MTL_COLOR_DEFAULT, ##args)

#define mtl_status(status) fprintf (stdout, "%s[%s %s %.3d] %s%s\n", \
	MTL_COLOR_RED, SRC_FILE_NAME, __FUNCTION__,\
	__LINE__, strerror(status), MTL_COLOR_DEFAULT)

#define mtl_errno() mtl_status(errno)

#define mtl_printq(s) {\
	auto __FILE____LINE__ = s.toLocal8Bit();\
	mtl_info("%s", __FILE____LINE__.data());\
}

#define mtl_printq2(msg, s) {\
	mtl_info("%s\"%s\"", msg, qPrintable(s));\
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

#define mtl_check(flag) {\
	if (!(flag)) {\
		return false;\
	}\
}

#define mtl_check_arg(flag, ret) {\
	if (!(flag)) {\
		return ret;\
	}\
}

#define mtl_check_void(flag) {\
	if (!(flag)) {\
		return;\
	}\
}

#define MTL_CHECK(flag) {\
	if (!(flag)) {\
		mtl_trace();\
		return false;\
	}\
}

#define MTL_CHECK_ARG(flag, ret) {\
	if (!(flag)) {\
		mtl_trace();\
		return ret;\
	}\
}

#define MTL_CHECK_VOID(flag) {\
	if (!(flag)) {\
		mtl_trace();\
		return;\
	}\
}

#define MTL_PTR_TO_INT(a) { (int)(i64)a }
#define MTL_INT_TO_PTR(i) { (isize*)(isize)i }

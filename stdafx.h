#ifndef __STDAFX_H__
#define __STDAFX_H__

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#ifdef _WIN32
#pragma once

#include "targetver.h"
#endif

#ifndef NDEBUG
#define CHECK_FOR_MEMORY_LEAKS
#endif

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:

#include <stdio.h>


#ifdef _WIN32
#include <tchar.h>
#include "ObjBase.h"
#else
typedef wchar_t _TCHAR;
#endif

// -- Goods Files and Directives
#define MURSIW_SUPPORT
#define PROHIBIT_UNSAFE_IMPLICIT_CASTS (0)
#define ENABLE_CONVERTIBLE_ASSIGNMENTS (0)

#include "goods.h"
#include "dbscls.h"

#define NO_USE_GOODS_TL_CACHE
#include "GoodsTLCache.h"

//#include "dbgoutput.h"

#include <string>

#endif

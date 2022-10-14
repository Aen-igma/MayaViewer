#pragma once

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define NEW new( _NORMAL_BLOCK, __FILENAME__, __LINE__)

#define _CRTDBG_MAP_ALLOC
#include<crtdbg.h>

#define SET_DEBUG_FLAGS _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF)
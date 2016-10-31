#pragma once

#if defined(_WIN32) || defined(WIN32)

	#define TARGET_PLATFORM_WINDOWS

	#if defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_PARTITION_APP
		#define TARGET_PLATFORM_WINDOWS_APP
	#else
		#define TARGET_PLATFORM_WINDOWS_DESKTOP
	#endif

#endif

#if defined(__ANDROID__) || defined(ANDROID)

	#define TARGET_PLATFORM_ANDROID

#endif

#if defined(__APPLE__)

#include "TargetConditionals.h"

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE

	#define TARGET_PLATFORM_IPHONE

	#if defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
		#define TARGET_PLATFORM_IPHONE_SIMULATOR	
	#else
		#define TARGET_PLATFORM_IPHONE_DEVICE
	#endif

#elif defined(__MACH__)

	#define TARGET_PLATFORM_MACOSX

#endif

#endif

#if defined(__linux__)

	#define TARGET_PLATFORM_LINUX

#endif

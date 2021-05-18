// === (C) 2020 === parallel_f / log (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include "system.hpp"
#include "ustd.hpp"


// parallel_f :: log == implementation

namespace parallel_f {

#define PARALLEL_F__LOG(Sym)												\
	do {																	\
		unsigned int ms = system::localtime_ms();											\
																			\
																			\
		char buf[1024];														\
																			\
		va_list args;														\
		va_start(args, fmt);												\
		vsnprintf(buf, sizeof(buf), fmt, args);								\
		va_end(args);														\
																			\
		std::stringstream tid; tid << std::this_thread::get_id();			\
																			\
		system::instance().log("(" #Sym ") [%02d:%02d:%02d.%03d] (%5s) %s",	\
							   ms/1000/60/60, ms/1000/60%60, ms/1000%60,			\
							   ms%1000, tid.str().c_str(), buf);	\
	} while (0)


//#define PARALLEL_F__NO_DEBUG

#ifndef PARALLEL_F__NO_DEBUG
#define LOG_DEBUG(...)		parallel_f::logDebug(__VA_ARGS__)
#else
#define LOG_DEBUG(...)		do {} while (0)
#endif

#define LOG_INFO(...)		parallel_f::logInfoF(__VA_ARGS__)


static inline void logDebug(const char* fmt, ...)
{
	if (getDebugLevel() == 0 && getDebugLevel(fmt) == 0)
		return;

	PARALLEL_F__LOG(-);
}

static inline void logDebugF(const char* fmt, ...)
{
	if (getDebugLevel() == 0 && getDebugLevel(fmt) == 0)
		return;

	PARALLEL_F__LOG(-);

	system::instance().flush();
}

static inline void logInfo(const char* fmt, ...)
{
	PARALLEL_F__LOG(*);
}

static inline void logInfoF(const char* fmt, ...)
{
	PARALLEL_F__LOG(*);

	system::instance().flush();
}

template <typename ArgType>
static inline std::string logString(ArgType arg)
{
	return ustd::to_string(arg);
}

template <>
static inline std::string logString(const char* arg)
{
	return arg;
}

template <typename... Args>
static inline void logLine(Args... args)
{
	std::string line;

	(line.append(logString(args)), ...);

	system::instance().log("%s\n", line.c_str());
}

template <typename... Args>
static inline void logLineF(Args... args)
{
	logLine<Args...>(args...);

	system::instance().flush();
}


}

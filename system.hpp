// === (C) 2020 === parallel_f / system (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <iostream>
#include <sstream>
#include <thread>

#include <stdarg.h>
#include <windows.h>


// parallel_f :: system == implementation

namespace parallel_f {

class system
{
public:
	static system& instance()
	{
		static system system_instance;

		return system_instance;
	}

private:
	int debug_level;
	std::stringstream slog;
	std::mutex llog;

public:
	system()
		:
		debug_level(0)
	{
	}

	~system()
	{
		flush();
	}

	int getDebugLevel()
	{
		return debug_level;
	}

	void setDebugLevel(int level)
	{
		debug_level = level;
	}

	void log(const char* fmt, ...)
	{
		char buf[1024];

		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);

		std::unique_lock<std::mutex> lock(llog);

		slog << buf;
	}

	void flush()
	{
		std::unique_lock<std::mutex> lock(llog);

		std::cerr << slog.str();

		slog = std::stringstream();
	}
};

static inline int getDebugLevel()
{
	return system::instance().getDebugLevel();
}

static inline void setDebugLevel(int level)
{
	system::instance().setDebugLevel(level);
}

static inline void logDebug(const char* fmt, ...)
{
	if (getDebugLevel() == 0)
		return;


	SYSTEMTIME ST;

	GetLocalTime(&ST);


	char buf[1024];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	std::stringstream tid; tid << std::this_thread::get_id();

	system::instance().log("(-) [%02d:%02d:%02d.%03d] (%5s) %s", ST.wHour, ST.wMinute, ST.wSecond, ST.wMilliseconds, tid.str().c_str(), buf);
}

static inline void logInfo(const char* fmt, ...)
{
	SYSTEMTIME ST;

	GetLocalTime(&ST);


	char buf[1024];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	std::stringstream tid; tid << std::this_thread::get_id();

	system::instance().log("(*) [%02d:%02d:%02d.%03d] (%5s) %s", ST.wHour, ST.wMinute, ST.wSecond, ST.wMilliseconds, tid.str().c_str(), buf);
}

}

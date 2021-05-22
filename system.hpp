// === (C) 2020/2021 === parallel_f / system (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <string.h>

#include <stdarg.h>
<<<<<<< HEAD
#include <string.h>

#ifdef _WIN32
=======

#ifdef _WIN32
#include <profileapi.h>
>>>>>>> e37acf9dbe2063ff1e7561c2c2cb5f3f436d3db4
#include <windows.h>
#else
#include <sys/time.h>
#endif


// parallel_f :: system == implementation

namespace parallel_f {

#ifdef _WIN32
class sysclock
{
private:
	LARGE_INTEGER frequency;
	LARGE_INTEGER last;

public:
	sysclock()
	{
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&last);
	}

	float reset()
	{
		LARGE_INTEGER current;

		QueryPerformanceCounter(&current);

		float ret = (current.QuadPart - last.QuadPart) / (float) frequency.QuadPart;

		last = current;

		return ret;
	}

	float get()
	{
		LARGE_INTEGER current;

		QueryPerformanceCounter(&current);

		float ret = (current.QuadPart - last.QuadPart) / (float) frequency.QuadPart;

	//	last = current;

		return ret;
	}
};
#else
class sysclock
{
private:
    long long last_us;

    static long long get_us()
    {
        struct timeval tv;

        gettimeofday(&tv, NULL);

        return tv.tv_sec * 1000000LL + (long long) tv.tv_usec;
    }

public:
	sysclock()
	{
	    last_us = get_us();
	}

	float reset()
	{
		long long current = get_us();

		float ret = (current - last_us) / 1000000.0f;

		last_us = current;

		return ret;
	}

	float get()
	{
		long long current = get_us();

		float ret = (current - last_us) / 1000000.0f;

	//	last_us = current;

		return ret;
	}
};
#endif


class system
{
public:
	static system& instance()
	{
		static system system_instance;

		return system_instance;
	}

	enum class AutoFlush {
		Never,
		Always,
		EndOfLine
	};

public:
	/* returns local time of day in ms (0-86399999) */
	static unsigned int localtime_ms()
	{
#ifdef _WIN32
		SYSTEMTIME ST;

		GetLocalTime(&ST);

		return ST.wHour * 60 * 60 * 1000 + ST.wMinute * 60 * 1000 + ST.wSecond * 1000 + ST.wMilliseconds;
#else
		struct timeval tv;

		gettimeofday(&tv, NULL);

		return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
	}

private:
    sysclock clock;
	int debug_level;
	std::map<std::string,int> debug_levels;
	std::stringstream slog;
	std::mutex llog;
	AutoFlush flog;

public:
    static float get_time()
    {
        return instance().clock.get();
    }

public:
	system()
		:
		debug_level(0),
		flog(AutoFlush::Never),
		flushThreadStop(false)
	{
	}

	~system()
	{
		stopFlushThread();

		flush();
	}

	int getDebugLevel()
	{
		return debug_level;
	}

	int getDebugLevel(std::string str)
	{
		for (auto it : debug_levels) {
			if (str.find(it.first) != str.npos)
				return it.second;
		}

		return 0;
	}

	void setDebugLevel(int level)
	{
		debug_level = level;
	}

	void setDebugLevel(std::string str, int level)
	{
		debug_levels[str] = level;
	}

public:
	void log(const char* fmt, ...)
	{
		char buf[1024];

		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, sizeof(buf), fmt, args);	// FIXME: handle return value (overflow etc)
		va_end(args);

		std::unique_lock<std::mutex> lock(llog);

		slog << buf;

		switch (flog) {
		case AutoFlush::EndOfLine:
			if (buf[strlen(buf) - 1] != '\n')
				return;
			/* fall through */
		case AutoFlush::Always:
			lock.unlock();
			flush();
			break;
		case AutoFlush::Never:
			break;
		}
	}

	void flush()
	{
		std::unique_lock<std::mutex> lock(llog);

		std::cerr << slog.str();

		slog = std::stringstream();
	}

	void setAutoFlush(AutoFlush auto_flush = AutoFlush::EndOfLine)
	{
		flog = auto_flush;
	}

private:
	bool flushThreadStop;
	std::unique_ptr<std::thread> flushThread;

public:
	void startFlushThread(unsigned int ms)
	{
		if (!flushThread) {
			flushThread = std::make_unique<std::thread>([this, ms]() {
				while (!flushThreadStop) {
					std::this_thread::sleep_for(std::chrono::milliseconds(ms));

					flush();
				}

				flush();
			});
		}
	}

	void stopFlushThread()
	{
		if (flushThread) {
			flushThreadStop = true;

			flushThread->join();

			flushThread.reset();
		}
	}
};

static inline int getDebugLevel()
{
	return system::instance().getDebugLevel();
}

static inline int getDebugLevel(std::string str)
{
	return system::instance().getDebugLevel(str);
}

static inline void setDebugLevel(int level)
{
	system::instance().setDebugLevel(level);
}

static inline void setDebugLevel(std::string str, int level)
{
	system::instance().setDebugLevel(str, level);
}


}

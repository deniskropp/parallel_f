// === (C) 2020 === parallel_f / system (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <iostream>
#include <mutex>
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

	enum class AutoFlush {
		Never,
		Always,
		EndOfLine
	};

private:
	int debug_level;
	std::map<std::string,int> debug_levels;
	std::stringstream slog;
	std::mutex llog;
	AutoFlush flog;

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
		vsnprintf(buf, sizeof(buf), fmt, args);
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

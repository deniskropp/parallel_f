// === (C) 2020/2021 === parallel_f / system (tasks, queues, lists in parallel
// threads) Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

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
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

// parallel_f :: system == implementation

namespace parallel_f {

#ifdef _WIN32
class sysclock {
private:
  LARGE_INTEGER frequency;
  LARGE_INTEGER last;

public:
  sysclock() {
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&last);
  }

  float reset() {
    LARGE_INTEGER current;

    QueryPerformanceCounter(&current);

    float ret = (current.QuadPart - last.QuadPart) / (float)frequency.QuadPart;

    last = current;

    return ret;
  }

  float get() {
    LARGE_INTEGER current;

    QueryPerformanceCounter(&current);

    float ret = (current.QuadPart - last.QuadPart) / (float)frequency.QuadPart;

    //	last = current;

    return ret;
  }
};
#else
class sysclock {
private:
  long long last_us;

  static long long get_us() {
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000000LL + (long long)tv.tv_usec;
  }

public:
  sysclock() { last_us = get_us(); }

  float reset() {
    long long current = get_us();

    float ret = (current - last_us) / 1000000.0f;

    last_us = current;

    return ret;
  }

  float get() {
    long long current = get_us();

    float ret = (current - last_us) / 1000000.0f;

    //	last_us = current;

    return ret;
  }
};
#endif

class system {
public:
  static system &instance() {
    static system system_instance;

    return system_instance;
  }

  enum class AutoFlush { Never, Always, EndOfLine };

public:
  /* returns local time of day in ms (0-86399999) */
  static unsigned int localtime_ms() {
#ifdef _WIN32
    SYSTEMTIME ST;

    GetLocalTime(&ST);

    return ST.wHour * 60 * 60 * 1000 + ST.wMinute * 60 * 1000 +
           ST.wSecond * 1000 + ST.wMilliseconds;
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
  }

private:
  sysclock clock;
  int debug_level;
  std::map<std::string, int> debug_levels;
  std::stringstream slog;
  std::mutex llog;
  AutoFlush flog;

public:
  static float get_time() { return instance().clock.get(); }

public:
  system() : debug_level(0), flog(AutoFlush::Never), flush_thread_stop(false) {}

  ~system() {
    stop_flush_thread();

    flush();
  }

  int get_debug_level() { return debug_level; }

  int get_debug_level(std::string str) {
    for (auto it : debug_levels) {
      if (str.find(it.first) != str.npos)
        return it.second;
    }

    return 0;
  }

  void set_debug_level(int level) { debug_level = level; }

  void set_debug_level(std::string str, int level) {
    debug_levels[str] = level;
  }

public:
  void log(const char *fmt, ...) {
    char buf[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt,
              args); // FIXME: handle return value (overflow etc)
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

  void flush() {
    std::unique_lock<std::mutex> lock(llog);

    std::cerr << slog.str();

    slog = std::stringstream();
  }

  void set_auto_flush(AutoFlush auto_flush = AutoFlush::EndOfLine) {
    flog = auto_flush;
  }

private:
  bool flush_thread_stop;
  std::unique_ptr<std::thread> flush_thread;

public:
  void start_flush_thread(unsigned int ms) {
    if (!flush_thread) {
      flush_thread = std::make_unique<std::thread>([this, ms]() {
        while (!flush_thread_stop) {
          std::this_thread::sleep_for(std::chrono::milliseconds(ms));

          flush();
        }

        flush();
      });
    }
  }

  void stop_flush_thread() {
    if (flush_thread) {
      flush_thread_stop = true;

      flush_thread->join();

      flush_thread.reset();
    }
  }
};

static inline int get_debug_level() {
  return system::instance().get_debug_level();
}

static inline int get_debug_level(std::string str) {
  return system::instance().get_debug_level(str);
}

static inline void set_debug_level(int level) {
  system::instance().set_debug_level(level);
}

static inline void set_debug_level(std::string str, int level) {
  system::instance().set_debug_level(str, level);
}

} // namespace parallel_f

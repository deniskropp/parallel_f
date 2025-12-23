// === (C) 2020 === parallel_f / log (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include "system.hpp"
#include "ustd.hpp"

// parallel_f :: log == implementation

namespace parallel_f {

#define PARALLEL_F__LOG(Sym)                                                   \
  do {                                                                         \
    unsigned int ms = system::localtime_ms();                                  \
                                                                               \
    char buf[1024];                                                            \
                                                                               \
    va_list args;                                                              \
    va_start(args, fmt);                                                       \
    vsnprintf(buf, sizeof(buf), fmt, args);                                    \
    va_end(args);                                                              \
                                                                               \
    std::stringstream tid;                                                     \
    tid << std::this_thread::get_id();                                         \
                                                                               \
    system::instance().log("(" #Sym ") [%02d:%02d:%02d.%03d] (%5s) %s",        \
                           ms / 1000 / 60 / 60, ms / 1000 / 60 % 60,           \
                           ms / 1000 % 60, ms % 1000, tid.str().c_str(), buf); \
  } while (0)

#define PARALLEL_F__ENABLE_DEBUG

#ifndef NDEBUG
#define NDEBUG (0)
#endif

#ifdef PARALLEL_F__ENABLE_DEBUG
#if NDEBUG
#define PARALLEL_F__DEBUG_ENABLED (0)
#else
#define PARALLEL_F__DEBUG_ENABLED (1)
#endif
#else
#define PARALLEL_F__DEBUG_ENABLED (0)
#endif

#if PARALLEL_F__DEBUG_ENABLED
#define LOG_DEBUG(...) parallel_f::log_debug(__VA_ARGS__)
#else
#define LOG_DEBUG(...)                                                         \
  do {                                                                         \
  } while (0)
#endif

#define LOG_INFO(...) parallel_f::log_info_f(__VA_ARGS__)

#define LOG_ERROR(...) parallel_f::log_error(__VA_ARGS__)

static inline void log_debug(const char *fmt, ...) {
  if (get_debug_level() == 0 && get_debug_level(fmt) == 0)
    return;

  PARALLEL_F__LOG(-);
}

static inline void log_debug_f(const char *fmt, ...) {
  if (get_debug_level() == 0 && get_debug_level(fmt) == 0)
    return;

  PARALLEL_F__LOG(-);

  system::instance().flush();
}

static inline void log_info(const char *fmt, ...) { PARALLEL_F__LOG(*); }

static inline void log_info_f(const char *fmt, ...) {
  PARALLEL_F__LOG(*);

  system::instance().flush();
}

static inline void log_error(const char *fmt, ...) {
  PARALLEL_F__LOG(!!!);

  system::instance().flush();
}

template <typename ArgType> static inline std::string log_string(ArgType arg) {
  return ustd::to_string(arg);
}

template <> inline std::string log_string(const char *arg) { return arg; }

template <typename... Args> static inline void log_line(Args... args) {
  std::string line;

  (line.append(log_string(args)), ...);

  system::instance().log("%s\n", line.c_str());
}

template <typename... Args> static inline void log_line_f(Args... args) {
  log_line<Args...>(args...);

  system::instance().flush();
}

} // namespace parallel_f

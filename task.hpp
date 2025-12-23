// === (C) 2020-2024 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <tuple>

#include "log.hpp"

#include "task_info.hpp"

namespace parallel_f {

namespace core {

// parallel_f :: task == implementation

/*
 * A task represents a unit of work that can be executed in a parallel thread.
 *
 * A task can be created with a callable object (typically a lambda) and
 * arguments. The task stores the callable and its arguments internally and
 * can execute the callable with the stored arguments.
 *
 * The task also inherits from the task_info class which stores information
 * about the task, such as its callable and arguments.
 *
 * Inheritance:
 *   public task_info
 *
 * Attributes:
 *   private:
 *     Callable callable: the callable object stored internally
 *     std::tuple<Args...> args: the arguments stored internally
 *
 * Methods:
 *   public:
 *     task(Callable callable, Args&&... args): initializes the task with
 *       the callable and arguments
 *     virtual ~task(): destructor
 *   protected:
 *     virtual bool run(): executes the callable with the stored arguments
 */
template <typename Callable, typename... Args> class task : public task_info {
private:
  Callable callable;
  std::tuple<Args...> args;

public:
  task(Callable callable, Args &&...args)
      : task_info(callable, args...), callable(callable), args(args...) {
    LOG_DEBUG("task::task()\n");
  }

  virtual ~task() { LOG_DEBUG("task::~task()\n"); }

protected:
  virtual bool run() {
    LOG_DEBUG("task::run()...\n");

    if constexpr (std::is_void_v<std::invoke_result_t<Callable, Args...>>)
      std::apply(callable, args);
    else
      value = std::apply(callable, args);

    LOG_DEBUG("task::run() done.\n");

    return true;
  }
};

} // namespace core

using core::task;

} // namespace parallel_f

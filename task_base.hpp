// === (C) 2020-2024 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <mutex>

#include "log.hpp"

#include "Event.hxx"

namespace parallel_f {

// parallel_f :: task_base == implementation

/*
 *  This class implements a base class for tasks, that is, objects that
 *  implement a `run` function and have a state that can be queried.
 *
 *  The state of a task can be one of three states: CREATED, RUNNING, and
 *  FINISHED. The state transitions are as follows:
 *
 *      CREATED -> RUNNING      on call to `finish`, if `run` returns true
 *      RUNNING -> FINISHED     on call to `finish`, if `run` returns true
 *
 *  The `finish` function returns true if the task has completed,
 *  regardless of whether the task is still running or has completed. The
 *  `get_state` function returns the current state of the task.
 *
 *  The `run` function must be implemented by the derived class, and is
 *  called by the `finish` function when the task is in the CREATED state.
 *  The `run` function must return true if the task has completed, and
 *  false otherwise.
 *
 *  The `enter_state` function is used by the derived class to enter the
 *  FINISHED state, and is used internally by the finish function to
 *  transition from the CREATED state to the RUNNING state.
 *
 *  The `finished` event is dispatched when the task enters the FINISHED
 *  state, and is used by the caller of `finish` to wait for the task to
 *  complete, or to perform some action after the task has completed.
 */
class task_base {
public:
  enum class task_state { CREATED, RUNNING, FINISHED };

  parallel_f::events::event<int> finished;

private:
  task_state state;
  std::mutex mutex;

public:
  task_base() : state(task_state::CREATED) {
    LOG_DEBUG("task_base::task_base(%p)\n", this);
  }

  ~task_base() { LOG_DEBUG("task_base::~task_base(%p)\n", this); }

  task_state get_state() const { return state; }

  bool finish() {
    LOG_DEBUG("task_base::finish(%p)\n", this);

    std::unique_lock<std::mutex> lock(mutex);

    switch (state) {
    case task_state::CREATED:
      state = task_state::RUNNING;

      lock.unlock();

      if (run()) {
        enter_state(task_state::FINISHED);
        LOG_DEBUG("task_base::finish(%p) returning true\n", this);
        return true;
      }
      break;

    case task_state::RUNNING:
      return false;

    case task_state::FINISHED:
      LOG_DEBUG("task_base::finish(%p) returning true\n", this);
      return true;
    }

    LOG_DEBUG("task_base::finish(%p) returning false\n", this);

    return false;
  }

protected:
  void enter_state(task_state state) {
    LOG_DEBUG("task_base::enter_state(%p, %d)\n", this, state);

    std::unique_lock<std::mutex> lock(mutex);

    if (this->state == state)
      return;

    switch (state) {
    case task_state::FINISHED:
      if (this->state != task_state::RUNNING)
        throw std::runtime_error("not running");

      finished.dispatch(0);
      break;

    default:
      throw std::runtime_error("invalid transition");
    }

    this->state = state;
  }

  virtual bool run() = 0;
};

} // namespace parallel_f

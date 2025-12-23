// === (C) 2020-2024 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <any>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <tuple>

#include "log.hpp"
#include "stats.hpp"
#include "system.hpp"
#include "vthread.hpp"

#include "Event.hxx"

#include "joinable.hpp"
#include "task.hpp"
#include "task_node.hpp"

namespace parallel_f {

static const std::any none;

/**
 * Generates a task object using the provided callable and arguments.
 *
 * @param callable The function or callable object to be executed by the task.
 * @param args The arguments to be passed to the callable.
 *
 * @return A shared pointer to a task object that wraps the callable and
 * arguments.
 *
 * @throws None
 */
template <typename Callable, typename... Args>
auto make_task(Callable callable, Args... args) {
  return std::shared_ptr<task<Callable, Args...>>(
      new task<Callable, Args...>(callable, std::forward<Args>(args)...));
}

// parallel_f :: task_queue == implementation

class task_queue {
private:
  std::shared_ptr<details::task_node> first;
  std::shared_ptr<details::task_node> last;
  std::mutex mutex;

public:
  task_queue() {}

  void push(std::shared_ptr<task_base> task) {
    LOG_DEBUG("task_queue::push()\n");

    std::unique_lock<std::mutex> lock(mutex);

    if (first) {
      auto node = std::make_shared<details::task_node>("task", task, 1);

      last->add_to_notify(node);

      last = node;
    } else {
      auto node = std::make_shared<details::task_node>("first", task, 1);

      first = node;
      last = node;
    }
  }

  joinable exec(bool detached = false) {
    LOG_DEBUG("task_queue::exec(%s)\n", detached ? "true" : "false");

    std::unique_lock<std::mutex> lock(mutex);

    auto f = first;
    auto l = last;

    first.reset();
    last.reset();

    lock.unlock();

    f->notify();

    if (!detached) {
      l->join();

      return joinable();
    }

    return joinable([f, l]() { l->join(); });
  }
};

class task_queue_simple : public events::event_listener {
private:
  std::vector<std::shared_ptr<task_base>> tasks;
  std::mutex lock;

public:
  task_queue_simple() {}

  void push(std::shared_ptr<task_base> task) {
    LOG_DEBUG("task_queue_simple::push()\n");

    std::unique_lock<std::mutex> l(lock);

    tasks.push_back(task);
  }

  void exec() {
    LOG_DEBUG("task_queue_simple::exec()\n");

    std::unique_lock<std::mutex> l(lock);

    std::vector<std::shared_ptr<task_base>> q = tasks;

    tasks.clear();

    l.unlock();

    run(q);
  }

private:
  void run(std::vector<std::shared_ptr<task_base>> &q) {
    for (auto t : q) {
      if (!t->finish()) {
        std::mutex lock;
        std::condition_variable cond;
        bool finished = false;

        std::unique_lock<std::mutex> l(lock);

        t->finished.attach(this, [&lock, &cond, &finished](int) {
          std::unique_lock<std::mutex> l(lock);

          finished = true;

          cond.notify_one();
        });

        while (!finished && t->get_state() != task_base::task_state::FINISHED)
          cond.wait(l);
      }
    }
  }
};

// parallel_f :: task_list == implementation

typedef unsigned long long task_id;

class task_list {
private:
  task_id ids;
  std::map<task_id, std::shared_ptr<details::task_node>> nodes;
  std::mutex mutex;
  std::shared_ptr<details::task_node> flush_join;

public:
  task_list() : ids(0), flush_join(0) {
    LOG_DEBUG("task_list::task_list(%p)\n", this);
  }

  ~task_list() { LOG_DEBUG("task_list::~task_list(%p)\n", this); }

  task_id append(std::shared_ptr<task_base> task) {
    LOG_DEBUG("task_list::append( no dependencies )\n");

    std::unique_lock<std::mutex> lock(mutex);

    task_id id = ++ids;

    nodes[id] = std::make_shared<details::task_node>("task", task, 1);

    return id;
  }

  template <typename... Deps>
  task_id append(std::shared_ptr<task_base> task, Deps... deps) {
    LOG_DEBUG("task_list::append( %d dependencies )\n", sizeof...(deps));

    std::unique_lock<std::mutex> lock(mutex);

    task_id id = ++ids;

    std::shared_ptr<details::task_node> node =
        std::make_shared<details::task_node>(
            "task", task, (unsigned int)(1 + sizeof...(deps)));

    std::list<task_id> deps_list({deps...});

    for (auto li : deps_list) {
      LOG_DEBUG("task_list::append()  <- id %llu\n", li);

      auto dep = nodes.find(li);

      if (dep != nodes.end())
        (*dep).second->add_to_notify(node);
      else
        node->notify();
    }

    nodes[id] = node;

    return id;
  }

  joinable finish(bool detached = false) {
    LOG_DEBUG("task_list::finish(%s)\n", detached ? "true" : "false");

    std::unique_lock<std::mutex> lock(mutex);

    if (flush_join) {
      LOG_DEBUG("task_list::finish() joining previous flush...\n");
      flush_join->join();
      LOG_DEBUG("task_list::finish() joined previous flush.\n");
    }

    for (auto node : nodes) {
      if (node.second != flush_join)
        node.second->notify();
    }

    flush_join.reset();

    if (!detached) {
      for (auto node : nodes)
        node.second->join();

      nodes.clear();

      return joinable();
    }

    auto n = nodes;

    nodes.clear();

    return joinable([n]() {
      for (auto node : n)
        node.second->join();
    });
  }

  task_id flush() {
    LOG_DEBUG("task_list::flush()...\n");

    std::unique_lock<std::mutex> lock(mutex);

    std::shared_ptr<details::task_node> prev_flush_join = flush_join;

    if (flush_join)
      flush_join = std::make_shared<details::task_node>(
          "flush", make_task([]() {}), (unsigned int)(1 + nodes.size() - 1));
    else
      flush_join = std::make_shared<details::task_node>(
          "flush", make_task([]() {}), (unsigned int)(1 + nodes.size()));

    if (prev_flush_join) {
      LOG_DEBUG("task_list::flush() joining previous flush...\n");
      prev_flush_join->join();
      LOG_DEBUG("task_list::flush() joined previous flush.\n");
    }

    LOG_DEBUG("task_list::flush() flushing (notify) nodes...\n");

    for (auto node : nodes) {
      if (node.second != prev_flush_join) {
        node.second->add_to_notify(flush_join);

        node.second->notify();
      }
    }

    LOG_DEBUG("task_list::flush() clearing nodes...\n");

    nodes.clear();

    LOG_DEBUG("task_list::flush() clearing nodes done.\n");

    task_id flush_id = ++ids;

    nodes[flush_id] = flush_join;

    flush_join->notify();

    LOG_DEBUG("task_list::flush() done.\n");

    return flush_id;
  }

  size_t length() const { return nodes.size(); }
};

} // namespace parallel_f

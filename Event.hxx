#pragma once

#include <functional>
#include <mutex>
#include <set>
#include <vector>

namespace parallel_f {
namespace events {

class event_listener;

class event_base {
public:
  virtual void detach(event_listener *listener) = 0;
};

class event_listener {
private:
  std::set<event_base *> events;
  std::mutex events_mutex;

public:
  ~event_listener() {
    std::unique_lock<std::mutex> lock(events_mutex);

    while (!events.empty()) {
      auto e = *events.begin();

      e->detach(this);
    }
  }

  void add_event(event_base *event) {
    std::unique_lock<std::mutex> lock(events_mutex);

    events.insert(event);
  }

  void remove_event(event_base *event) { events.erase(event); }
};

template <typename... Args> class event : public event_base {
private:
  class handler {
  public:
    event_listener *listener;

    std::function<void(Args...)> func;

    handler(event_listener *listener, std::function<void(Args...)> func)
        : listener(listener), func(func) {}
  };

  std::vector<handler> handlers;
  std::mutex handlers_mutex;

public:
  void attach(event_listener *listener, std::function<void(Args...)> func) {
    std::unique_lock<std::mutex> lock(handlers_mutex);

    handlers.emplace_back(listener, func);

    listener->add_event(this);
  }

  virtual void detach(event_listener *listener) override {
    std::unique_lock<std::mutex> lock(handlers_mutex);

    std::vector<handler> h = handlers;

    handlers.clear();

    for (size_t i = 0; i < h.size(); i++) {
      if (h[i].listener != listener) {
        handlers.push_back(h[i]);
      }
    }

    listener->remove_event(this);
  }

  void dispatch(Args... args) {
    std::unique_lock<std::mutex> lock(handlers_mutex);

    for (auto &handler : handlers)
      handler.func(args...);
  }
};

} // namespace events
} // namespace parallel_f
#pragma once

#include <functional>
#include <mutex>
#include <set>
#include <vector>


namespace lli {


class EventListener;


class EventBase {
public:
    virtual void Detach(EventListener *listener) = 0;
};


class EventListener
{
private:
    std::set<EventBase*> events;
    std::mutex events_mutex;

public:
    ~EventListener()
    {
        std::unique_lock<std::mutex> lock(events_mutex);

        while (!events.empty())
        {
            auto e = *events.begin();

            e->Detach(this);
        }
    }

    void AddEvent(EventBase* event)
    {
        std::unique_lock<std::mutex> lock(events_mutex);

        events.insert(event);
    }

    void RemoveEvent(EventBase* event)
    {
        events.erase(event);
    }
};


template <typename... Args>
class Event : public EventBase
{
private:
    class Handler
    {
    public:
        EventListener *listener;

        std::function<void(Args...)>    func;

        Handler(EventListener* listener, std::function<void(Args...)> func)
            :
            listener(listener),
            func(func)
        {
        }
    };

    std::vector<Handler> handlers;
    std::mutex handlers_mutex;

public:
    void Attach(EventListener *listener, std::function<void(Args...)> func)
    {
        std::unique_lock<std::mutex> lock(handlers_mutex);

        handlers.emplace_back(listener, func);

        listener->AddEvent(this);
    }

    virtual void Detach(EventListener *listener) override
    {
        std::unique_lock<std::mutex> lock(handlers_mutex);

        std::vector<Handler> h = handlers;

        handlers.clear();

        for (size_t i=0; i<h.size(); i++) {
            if (h[i].listener != listener) {
                handlers.push_back(h[i]);
            }
        }

        listener->RemoveEvent(this);
    }

    void Dispatch(Args... args)
    {
        std::unique_lock<std::mutex> lock(handlers_mutex);

        for (auto& handler : handlers)
            handler.func(args...);
    }
};


}
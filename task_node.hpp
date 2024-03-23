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



#include "task.hpp"
#include "joinable.hpp"


namespace parallel_f {


class task_node : public lli::EventListener
{
private:
	std::shared_ptr<task_base> task;
	unsigned int wait;
	bool managed;
	std::shared_ptr<vthread> thread;
	std::mutex lock;
	std::condition_variable cond;
	bool finished;

public:
	task_node(std::string name, std::shared_ptr<task_base> task, unsigned int wait, bool managed = true)
		:
		task(task),
		wait(wait),
		managed(managed),
		finished(false)
	{
		LOG_DEBUG("task_node::task_node(%p, '%s', %u)\n", this, name.c_str(), wait);

		thread = std::make_shared<vthread>(name);

		task->finished.Attach(this, [this](int) {
			std::unique_lock<std::mutex> l(lock);

			finished = true;

			cond.notify_all();
		});
	}

	~task_node()
	{
		LOG_DEBUG("task_node::~task_node(%p '%s')\n", this, get_name().c_str());
	}

	void add_to_notify(std::shared_ptr<task_node> node)
	{
		LOG_DEBUG("task_node::add_to_notify(%p '%s', %p)\n", this, get_name().c_str(), node.get());

        if (finished)
            throw std::runtime_error("finished");

        task->finished.Attach(node.get(), [node](int) {
			node->notify();
		});
	}

	void notify()
	{
		LOG_DEBUG("task_node::notify(%p '%s')...\n", this, get_name().c_str());

		std::unique_lock<std::mutex> l(lock);

		LOG_DEBUG("task_node::notify(%p '%s') wait count %u -> %u\n", this, get_name().c_str(), wait, wait-1);

		if (!wait)
			throw std::runtime_error("zero wait count");

		if (!--wait) {
			std::shared_ptr<task_base> t = task;

			thread->start([t]() {
				bool finished = t->finish();
			}, managed);
		}

		LOG_DEBUG("task_node::notify(%p '%s') done.\n", this, get_name().c_str());
	}

	void join()
	{
		LOG_DEBUG("task_node::join(%p '%s')...\n", this, get_name().c_str());

		std::unique_lock<std::mutex> l(lock);

		while (!finished) {
			LOG_DEBUG("task_node::join(%p '%s') waiting...\n", this, get_name().c_str());

			vthread::wait(cond, l);
		}

		LOG_DEBUG("task_node::join(%p '%s') done.\n", this, get_name().c_str());
	}

	std::thread::id get_thread_id()
	{
		return thread->get_id();
	}

	std::string get_name()
	{
		return thread->get_name();
	}
};


}

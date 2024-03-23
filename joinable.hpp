// === (C) 2020-2024 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <functional>
#include <list>

#include "log.hpp"



namespace parallel_f {


// parallel_f :: task_queue == implementation

class joinable
{
	friend class task_queue;
	friend class task_list;

private:
	std::function<void(void)> join_f;

	joinable(std::function<void(void)> join_f)
		:
		join_f(join_f)
	{
	}

public:
	joinable() {}

public:
	void join()
	{
		LOG_DEBUG("joinable::join()\n");

		if (join_f)
			join_f();
	}
};

class joinables
{
private:
	std::list<joinable> list;

public:
	void add(joinable joinable)
	{
		list.push_back(joinable);
	}

	void join_all()
	{
		for (auto j : list)
			j.join();
	}
};


}

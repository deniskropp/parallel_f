// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <iostream>
#include <list>
#include <memory>

#ifdef _WIN32
#include <profileapi.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "system.hpp"


// parallel_f :: stats == implementation

namespace parallel_f {


namespace stats {

class stat
{
private:
	std::string name;
	float seconds_busy;
	float seconds_idle;
	unsigned int num;

public:
	stat(std::string name) : name(name)
	{
		reset();
	}

	void report_busy(float seconds)
	{
		seconds_busy += seconds;

		num++;
	}

	void report_idle(float seconds)
	{
		seconds_idle += seconds;
	}

	float get_load() const
	{
		if (seconds_busy + seconds_idle)
			return seconds_busy / (seconds_busy + seconds_idle);

		return 0.0f;
	}

	float get_busy() const
	{
		return seconds_busy;
	}

	std::string get_name() const
	{
		return name;
	}

	unsigned int get_num() const
	{
		return num;
	}

	void reset()
	{
		seconds_busy = 0.0f;
		seconds_idle = 0.0f;
		num = 0;
	}
};


class instance
{
public:
	static instance& get()
	{
		static instance Instance;

		return Instance;
	}

private:
	std::mutex lock;
	std::list<std::shared_ptr<stat>> stats;
	sysclock total;

private:
	instance() {}

public:
	std::shared_ptr<stat> make_stat(std::string name)
	{
		std::unique_lock<std::mutex> l(lock);

		auto s = std::make_shared<stat>(name);

		stats.push_back(s);

		stats.sort([] (auto a, auto b) { return a->get_name() < b->get_name(); });

		return s;
	}

	void show_stats()
	{
		std::unique_lock<std::mutex> l(lock);

		float total_seconds = total.reset();

		std::map<std::string, std::list<std::shared_ptr<stat>>> groups;

		for (auto s : stats) {
			std::string group = s->get_name().substr(0, s->get_name().find("."));

			groups[group].push_back(s);
		}

		for (auto g : groups) {
			float total_load = 0.0f;
			float total_busy = 0.0f;
			unsigned int total_num = 0;

			for (auto s : g.second) {
				system::instance().log("Load '%s': %.3f (%u vthreads)\n",
									   s->get_name().c_str(), s->get_load(), s->get_num());

				total_load += s->get_load();
				total_busy += s->get_busy();
				total_num += s->get_num();

				s->reset();
			}

			system::instance().log("Load '%s' (all): %.3f (%u vthreads), total busy %.3f%% (%.3f seconds)\n",
								   g.first.c_str(), total_load, total_num, (total_busy / total_seconds) * 100.0f,
								   total_seconds);
		}
	}
};

}

}

// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <iostream>
#include <list>
#include <memory>

#include <windows.h>

#include "system.hpp"


// parallel_f :: stats == implementation

namespace parallel_f {

class sysclock
{
private:
	SYSTEMTIME last;

public:
	sysclock()
	{
		GetLocalTime(&last);
	}

	float reset()
	{
		SYSTEMTIME current;

		GetLocalTime(&current);

		float ret = (current.wHour - last.wHour) * 60.0f * 24.0f +
			(current.wMinute - last.wMinute) * 60.0f +
			(current.wSecond - last.wSecond) +
			(current.wMilliseconds - last.wMilliseconds) / 1000.0f;

		last = current;

		return ret;
	}
};

	
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
	std::list<std::shared_ptr<stat>> stats;
	sysclock total;

public:
	std::shared_ptr<stat> make_stat(std::string name)
	{
		auto s = std::make_shared<stat>(name);

		stats.push_back(s);

		stats.sort([] (auto a, auto b) { return a->get_name() < b->get_name(); });

		return s;
	}

	void show_stats()
	{
		float total_load = 0.0f;
		float total_busy = 0.0f;
		unsigned int total_num = 0;

		for (auto s : stats) {
			system::instance().log("Load '%s': %.3f (%u vthreads)\n", s->get_name().c_str(), s->get_load(), s->get_num());

			total_load += s->get_load();
			total_busy += s->get_busy();
			total_num += s->get_num();

			s->reset();
		}
	
		float total_seconds = total.reset();

		system::instance().log("Load all: %.3f (%u vthreads), total busy %.3f%% (%.3f seconds)\n",
							   total_load, total_num, (total_busy / total_seconds) * 100.0f, total_seconds);
	}
};

}

}
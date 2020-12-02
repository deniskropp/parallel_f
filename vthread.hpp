// === (C) 2020 === parallel_f / vthread (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "stats.hpp"
#include "system.hpp"


namespace parallel_f {


// parallel_f :: vthread == implementation

class vthread
{
private:
	std::string name;
	std::function<void(void)> func;
	bool done;
	std::mutex mutex;
	std::condition_variable cond;
	std::thread::id thread_id;
	std::thread* unmanaged;

	class manager
	{
	private:
		std::map<std::string, unsigned int> names;
		std::mutex mutex;
		std::condition_variable cond;
		std::queue<vthread*> queue;
		std::vector<std::thread*> threads;
		int running;
		bool shutdown;

	public:
		static manager& instance()
		{
			static manager manager_instance;

			return manager_instance;
		}

	public:
		manager()
			:
			running(0),
			shutdown(false)
		{
			for (int i = 0; i < 5; i++) {
				auto stat = stats::instance::get().make_stat(std::to_string(i));

				threads.push_back(new std::thread([this,stat]() { loop(stat); }));
			}
		}

		~manager()
		{
			logDebug("~manager(): shutting down...\n");

			std::unique_lock<std::mutex> lock(mutex);
			
			shutdown = true;

			cond.notify_all();

			lock.unlock();

			for (auto t : threads) {
				logDebug("joining thread...\n");

				t->join();

				delete t;
			}
		}

		std::string make_name(std::string name)
		{
			return name + "." + std::to_string(names[name]++);
		}

		void loop(std::shared_ptr<stats::stat> stat)
		{
			while (!shutdown)
				once(stat);
		}

		void once( std::shared_ptr<stats::stat> stat, unsigned int timeout_ms = 100 )
		{
			sysclock clock;
			std::unique_lock<std::mutex> lock(mutex);

			if (queue.empty())
				cond.wait_for(lock, std::chrono::milliseconds(timeout_ms));

			if (stat)
				stat->report_idle(clock.reset());
			
			if (shutdown || queue.empty())
				return;

			vthread* t = queue.front();

			queue.pop();

			running++;

			logDebug("running: %d, queue length: %zu\n", running, queue.size());

			lock.unlock();

			t->run();

			lock.lock();

			running--;

			logDebug("running: %d, queue length: %zu\n", running, queue.size());

			if (stat)
				stat->report_busy(clock.reset());
		}

		void schedule(vthread* thread)
		{
			std::unique_lock<std::mutex> lock(mutex);

			queue.push(thread);

			cond.notify_one();
		}

		bool has_thread(std::thread::id tid)
		{
			for (auto t : threads) {
				if (t->get_id() == tid)
					return true;
			}

			return false;
		}
	};

public:
	static void yield()
	{
		logDebug("vthread::yield()...\n");

		manager::instance().once(0, 10);
	}

	static bool is_manager_thread()
	{
		return manager::instance().has_thread(std::this_thread::get_id());
	}

public:
	vthread(std::string name = "unnamed")
		:
		name(manager::instance().make_name(name)),
		done(false),
		unmanaged(0)
	{
	}

	~vthread()
	{
		if (unmanaged) {
			unmanaged->join();
			delete unmanaged;
		}
	}

	void start(std::function<void(void)> func, bool managed = true)
	{
		logDebug("vthread::start('%s')...\n", name.c_str());

		if (this->func)
			throw std::runtime_error("vthread::start called again");

		this->func = func;

		if (managed)
			manager::instance().schedule(this);
		else {
			unmanaged = new std::thread([this]() {
					run();
				});
		}
	}

	void run()
	{
		logDebug("vthread::run('%s')...\n", name.c_str());

		thread_id = std::this_thread::get_id();

		func();

		std::unique_lock<std::mutex> lock(mutex);
		
		logDebug("vthread::run('%s') finished.\n", name.c_str());

		done = true;

		cond.notify_all();
	}

	void join()
	{
		logDebug("vthread::join('%s')...\n",name.c_str());

		std::unique_lock<std::mutex> lock(mutex);

		while (!done) {
			if (vthread::is_manager_thread()) {
				if (thread_id == std::this_thread::get_id())
					throw std::runtime_error("calling join on ourself");

				lock.unlock();

				vthread::yield();

				lock.lock();
			}
			else
				cond.wait(lock);
		}
	}

	std::thread::id get_id()
	{
		return thread_id;
	}

	std::string get_name()
	{
		return name;
	}
};

}
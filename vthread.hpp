// === (C) 2020 === parallel_f / vthread (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stack>
#include <string>
#include <thread>

#include "stats.hpp"
#include "system.hpp"


namespace parallel_f {


// parallel_f :: vthread == implementation

class vthread : public std::enable_shared_from_this<vthread>
{
private:
	class manager
	{
	private:
		std::map<std::string, unsigned int> names;
		std::mutex mutex;
		std::condition_variable cond;
		std::stack<std::shared_ptr<vthread>> stack;
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
			for (unsigned int i = 0; i < std::thread::hardware_concurrency(); i++) {
				auto stat = stats::instance::get().make_stat(std::string("cpu.") + std::to_string(i));

				threads.push_back(new std::thread([this,stat]() { loop(stat); }));
			}
		}

		~manager()
		{
			LOG_DEBUG("vthread::manager::~manager(): shutting down...\n");

			std::unique_lock<std::mutex> lock(mutex);
			
			shutdown = true;

			cond.notify_all();

			lock.unlock();

			for (auto t : threads) {
				LOG_DEBUG("vthread::manager::~manager(): joining thread...\n");

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

			if (stack.empty())
				cond.wait_for(lock, std::chrono::milliseconds(timeout_ms));

			if (stat)
				stat->report_idle(clock.reset());
			
			if (shutdown || stack.empty())
				return;

			std::shared_ptr<vthread> t = stack.top();

			stack.pop();

			running++;

			LOG_DEBUG("vthread::manager::once(): running: %d, stack: %zu\n", running, stack.size());

			lock.unlock();

			t->run();

			lock.lock();

			running--;

			if (stat)
				stat->report_busy(clock.reset());
		}

		void schedule(std::shared_ptr<vthread> thread)
		{
			std::unique_lock<std::mutex> lock(mutex);

			stack.push(thread);

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
	static bool is_managed_thread()
	{
		return manager::instance().has_thread(std::this_thread::get_id());
	}

	static void yield()
	{
		LOG_DEBUG("vthread::yield()...\n");

		if (!is_managed_thread())
			throw std::runtime_error("not a managed thread");

		manager::instance().once(0, 10);
	}

	static void wait(std::condition_variable &cond, std::unique_lock<std::mutex> &lock)
	{
		LOG_DEBUG("vthread::wait()...\n");

		if (is_managed_thread())
			throw std::runtime_error("illegal wait in managed thread");

		cond.wait(lock);
	}

private:
	std::string name;
	std::function<void(void)> func;
	bool done;
	std::mutex mutex;
	std::condition_variable cond;
	std::thread::id thread_id;
	std::thread* unmanaged;

public:
	vthread(std::string name = "unnamed")
		:
		name(manager::instance().make_name(name)),
		done(false),
		unmanaged(0)
	{
		LOG_DEBUG("vthread::vthread(%p, '%s')\n", this, name.c_str());
	}

	~vthread() noexcept(false)
	{
		LOG_DEBUG("vthread::~vthread(%p '%s')...\n", this, name.c_str());

		std::unique_lock<std::mutex> lock(mutex);

		if (func) {	// thread had been started?
			LOG_DEBUG("vthread::~vthread(%p '%s') thread was started\n", this, name.c_str());

			while (!done) {
				if (vthread::is_managed_thread())
					throw std::runtime_error("~vthread while running");

				LOG_DEBUG("vthread::~vthread(%p '%s') waiting for run() to finish\n", this, name.c_str());

				vthread::wait(cond, lock);
			}

			LOG_DEBUG("vthread::~vthread(%p '%s') thread is done\n", this, name.c_str());
		}

		if (unmanaged) {
			unmanaged->join();
			delete unmanaged;
		}
	}

public:
	void start(std::function<void(void)> f, bool managed = true)
	{
		LOG_DEBUG("vthread::start(%p '%s', %s)...\n", this, name.c_str(), f.target_type().name());

		std::unique_lock<std::mutex> lock(mutex);

		if (func)
			throw std::runtime_error("vthread::start called again");

		func = f;

		auto shared_this = shared_from_this();

		if (managed) {
			manager::instance().schedule(shared_this);
		}
		else {
			unmanaged = new std::thread([shared_this]() {
				shared_this->run();
			});
		}
	}

	void run()
	{
		LOG_DEBUG("vthread::run(%p '%s')...\n", this, name.c_str());

		std::unique_lock<std::mutex> lock(mutex);

		thread_id = std::this_thread::get_id();

		std::function<void(void)> f = func;

		lock.unlock();

		LOG_DEBUG("vthread::run(%p '%s') calling %s...\n", this, name.c_str(), f.target_type().name());

		if (f)
			f();

		LOG_DEBUG("vthread::run(%p '%s') calling %s done.\n", this, name.c_str(), f.target_type().name());

		lock.lock();

		done = true;

		cond.notify_all();

		thread_id = std::thread::id();

		LOG_DEBUG("vthread::run(%p '%s') done.\n", this, name.c_str());
	}

	void join()
	{
		LOG_DEBUG("vthread::join(%p '%s')...\n", this, name.c_str());

		std::unique_lock<std::mutex> lock(mutex);

		while (!done) {
			if (vthread::is_managed_thread()) {
				if (thread_id == std::this_thread::get_id())
					throw std::runtime_error("calling join on ourself");

				lock.unlock();

				vthread::yield();	// TODO: implement condition_variable support

				lock.lock();
			}
			else
				vthread::wait(cond, lock);
		}
	}

	std::thread::id get_id() const
	{
		return thread_id;
	}

	std::string get_name() const
	{
		return name;
	}
};

}
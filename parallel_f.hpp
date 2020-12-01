// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <any>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <tuple>

#include <stdarg.h>
#include <windows.h>

#include "stats.hpp"


std::string PrintF(const char* fmt, ...)
{
	char buf[1024];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	return buf;
}

// parallel_f :: system == implementation

namespace parallel_f {

class system
{
public:
	static system& instance()
	{
		static system system_instance;

		return system_instance;
	}

private:
	int debug_level;

public:
	system()
		:
		debug_level(0)
	{
	}

	int getDebugLevel()
	{
		return debug_level;
	}

	void setDebugLevel(int level)
	{
		debug_level = level;
	}
};

static inline int getDebugLevel()
{
	return system::instance().getDebugLevel();
}

static inline void setDebugLevel(int level)
{
	system::instance().setDebugLevel(level);
}

static inline void logDebug(const char* fmt, ...)
{
	if (system::instance().getDebugLevel() == 0)
		return;


	SYSTEMTIME ST;

	GetLocalTime(&ST);


	char buf[1024];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	std::stringstream tid; tid << std::this_thread::get_id();

	fprintf( stderr, "[%02d:%02d:%02d.%03d] (%5s) %s",
			 ST.wHour, ST.wMinute, ST.wSecond, ST.wMilliseconds, tid.str().c_str(), buf );
}

static inline void logInfo(const char* fmt, ...)
{
	SYSTEMTIME ST;

	GetLocalTime(&ST);


	char buf[1024];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	std::stringstream tid; tid << std::this_thread::get_id();

	fprintf(stderr, "[%02d:%02d:%02d.%03d] (%5s) %s",
		ST.wHour, ST.wMinute, ST.wSecond, ST.wMilliseconds, tid.str().c_str(), buf);
}

}


// parallel_f :: task_base == implementation

namespace parallel_f {

class task_base
{
private:
	enum class task_state {
		CREATED,
		FINISHED
	};

	task_state state;
	std::mutex mutex;

public:
	task_base() : state(task_state::CREATED) {}

	void reset()
	{
		std::unique_lock<std::mutex> lock(mutex);

		state = task_state::CREATED;
	}
	
	void finish()
	{
		std::unique_lock<std::mutex> lock(mutex);

		if (state == task_state::CREATED) {
			state = task_state::FINISHED;

			// TODO: keep locked while running task?
			lock.unlock();

			run();
		}
	}

protected:
	virtual void run() = 0;
};


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
			return PrintF("%s.%u", name.c_str(), names[name]++);
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
		done(false)
	{
	}

	void start(std::function<void(void)> func)
	{
		logDebug("vthread::start('%s')...\n", name.c_str());

		this->func = func;

		manager::instance().schedule(this);
//		(new std::thread([this]() {run(); }))->detach();
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


// parallel_f :: task_queue == implementation

class joinable
{
	friend class task_queue;

private:
	vthread* thread;

	joinable(vthread* thread = NULL)
		:
		thread(thread)
	{
	}

public:
	void join()
	{
		if (thread)
			thread->join();
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

class task_queue
{
private:
	std::queue<std::shared_ptr<task_base>> tasks;
	std::mutex mutex;

public:
	task_queue() {}

	void push(std::shared_ptr<task_base> task, bool reset = true)
	{
		std::unique_lock<std::mutex> lock(mutex);

		if (reset)
			task->reset();

		tasks.push(task);
	}

	joinable exec(bool detached = false)
	{
		std::unique_lock<std::mutex> lock(mutex);

		std::queue<std::shared_ptr<task_base>> fq = tasks;

		tasks = std::queue<std::shared_ptr<task_base>>();

		lock.unlock();

		if (detached) {
			vthread* t = new vthread();
			
			t->start([fq]() {
				finish_all(fq);
			});

			return joinable(t);
		}

		finish_all(fq);

		return joinable();
	}

private:
	static void finish_all(std::queue<std::shared_ptr<task_base>> fq)
	{
		while (!fq.empty()) {
			auto task = fq.front();

			fq.pop();

			task->finish();
		}
	}
};


// parallel_f :: task == implementation

class task_info : public task_base, public std::enable_shared_from_this<task_info>
{
public:
	class Value
	{
		friend task_info;

	private:
		std::shared_ptr<task_info> task;

	public:
		Value(std::shared_ptr<task_info> task) : task(task) {}

		template <typename _T>
		_T get() {
			return std::any_cast<_T>(task->value);
		}

		template <>
		std::any get() {
			return task->value;
		}
	};

protected:
	std::any value;

protected:
	task_info() {}

	template <typename Callable, typename... Args>
	task_info(Callable callable, Args... args)
	{
		if (sizeof...(args)) {
			logDebug("[[%s]], showing %zu arguments...\n", typeid(Callable).name(), sizeof...(args));

			if (getDebugLevel() > 0)
				(DumpArg<Args>(args), ...);
		}
		else
			logDebug("[[%s]], no arguments...\n", typeid(Callable).name());
	}

public:
	Value result() { return Value(this->shared_from_this()); }

private:
	template <typename ArgType>
	void DumpArg(ArgType arg)
	{
		logDebug("  Type: %s\n", typeid(ArgType).name());
	}

	template <>
	void DumpArg(Value arg)
	{
		switch (parallel_f::getDebugLevel()) {
		case 2:
			arg.task->finish();
		case 1:
			logDebug("  Type: Value( %s )\n", arg.get<std::any>().type().name());
			break;
		case 0:
			break;
		}
	}
};

template <typename Callable, typename... Args>
class task : public task_info
{
private:
	Callable callable;
	std::tuple<Args...> args;

public:
	task(Callable callable, Args... args)
		:
		task_info(callable, args...),
		callable(callable),
		args(args...)
	{
	}

protected:
	virtual void run()
	{
		value = std::apply(callable, args);
	}
};

static const std::any none;

template <typename Callable, typename... Args>
auto make_task(Callable callable, Args... args)
{
	return std::shared_ptr<task<Callable, Args...>> (new task<Callable, Args...>(callable, args...));
}


// parallel_f :: task_list == implementation

typedef unsigned long long task_id;

class task_list
{
private:
	class task_node
	{
	private:
		std::shared_ptr<task_base> task;
		unsigned int wait;
		vthread thread;
		std::list<task_node*> to_notify;

	public:
		task_node(std::string name, std::shared_ptr<task_base> task, unsigned int wait)
			:
			task(task),
			wait(wait),
			thread(name)
		{
		}

		void add_to_notify(task_node* node)
		{
			to_notify.push_back(node);
		}

		void notify()
		{
			if (!--wait) {
				thread.start([this]() {
					task->finish();

					for (auto node : to_notify)
						node->notify();
				});
			}
		}

		void join()
		{
			thread.join();
		}

		std::thread::id get_thread_id()
		{
			return thread.get_id();
		}

		std::string get_name()
		{
			return thread.get_name();
		}
	};

private:
	task_id ids;
	std::map<task_id, task_node*> nodes;
	std::mutex mutex;
	task_node *flush_join;

public:
	task_list()
		:
		ids(0),
		flush_join(0)
	{
	}

	task_id append(std::shared_ptr<task_base> task)		// TODO: reset option?
	{
		logDebug( "task_list::append( no dependencies )\n" );

		std::unique_lock<std::mutex> lock(mutex);

		task_id id = ++ids;

		nodes[id] = new task_node("task", task, 1);

		return id;
	}

	template <typename ...Deps>
	task_id append(std::shared_ptr<task_base> task, Deps... deps)		// TODO: reset option?
	{
		logDebug( "task_list::append( %d dependencies )\n", sizeof...(deps) );

		std::unique_lock<std::mutex> lock(mutex);

		task_id id = ++ids;

		task_node* node = new task_node("task", task, 1 + sizeof...(deps));

		std::list<task_id> deps_list({ deps... });

		for (auto li : deps_list) {
			logDebug("  <- id %llu\n", li);

			if (li)
				nodes[li]->add_to_notify(node);
			else
				node->notify();
		}

		nodes[id] = node;

		return id;
	}

	void finish()
	{
		logDebug( "task_list::finish()\n" );

		std::unique_lock<std::mutex> lock(mutex);

		for (auto node : nodes)
			node.second->notify();

		for (auto node : nodes) {
			node.second->join();
			
			delete node.second;
		}
		
		flush_join = NULL;

		nodes.clear();
	}

	task_id flush()
	{
		logDebug("task_list::flush()\n");

		std::unique_lock<std::mutex> lock(mutex);

		std::list<task_node*> flushed;

		for (auto node : nodes) {
			std::stringstream ss; ss << node.second->get_thread_id();

			logDebug("  -> notify %s ('%s')\n", ss.str().c_str(), node.second->get_name().c_str());

			node.second->notify();

			if (node.second != flush_join) {
				logDebug("      pushing for join\n");

				flushed.push_back(node.second);
			}
		}

		if (flush_join) {
			flush_join->join();
			delete flush_join;
		}

		nodes.clear();

		auto task = make_task([flushed]() {
			for (auto node : flushed) {
				node->join();

				delete node;
			}

			return none;
		});

		lock.unlock();

		flush_join = new task_node("flush", task, 1);

		task_id id = ++ids;

		nodes[id] = flush_join;
		
		return id;
	}

	size_t length() const
	{
		return nodes.size();
	}
};

}
// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <any>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <tuple>

#include "stats.hpp"
#include "system.hpp"
#include "vthread.hpp"


namespace parallel_f {


// parallel_f :: task_base == implementation

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
	task(Callable callable, Args&&... args)
		:
		task_info(callable, args...),
		callable(callable),
		args(args...)
	{
	}

protected:
	virtual void run()
	{
		if constexpr (std::is_void_v<std::invoke_result_t<Callable,Args...>>)
			std::apply(callable, args);
		else
			value = std::apply(callable, args);
	}
};

static const std::any none;

template <typename Callable, typename... Args>
auto make_task(Callable callable, Args... args)
{
	return std::shared_ptr<task<Callable, Args...>> (new task<Callable,Args...>(callable, std::forward<Args>(args)...));
}


// parallel_f :: task_queue == implementation

class joinable
{
	friend class task_queue;
	friend class task_list;

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
		bool managed;

	public:
		task_node(std::string name, std::shared_ptr<task_base> task, unsigned int wait, bool managed = true)
			:
			task(task),
			wait(wait),
			thread(name),
			managed(managed)
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
				}, managed);
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

	joinable finish(bool detached = false)
	{
		logDebug( "task_list::finish()\n" );

		std::unique_lock<std::mutex> lock(mutex);

		auto n = nodes;

		nodes.clear();

		flush_join = NULL;

		if (detached) {
			vthread* thread = new vthread("finish");

			thread->start([n]() {
					for (auto node : n)
						node.second->notify();

					for (auto node : n) {
						node.second->join();

						delete node.second;
					}
				});

			return joinable(thread);
		}
		else {
			for (auto node : n)
				node.second->notify();
				
			for (auto node : n) {
				node.second->join();

				delete node.second;
			}
		}

		return joinable();
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

		flush_join = new task_node("flush", task, 1, false);

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
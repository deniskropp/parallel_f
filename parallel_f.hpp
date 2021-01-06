// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
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


namespace parallel_f {


// parallel_f :: task_base == implementation

class task_base
{
public:
	enum class task_state {
		CREATED,
		RUNNING,
		FINISHED
	};

private:
	task_state state;
	std::mutex mutex;
	std::vector<std::function<void(void)>> on_finished;

public:
	task_base() : state(task_state::CREATED) {}

	~task_base() {}

	size_t handle_finished(std::function<void(void)> f)
	{
		std::unique_lock<std::mutex> lock(mutex);
		
		size_t index = on_finished.size();

		on_finished.push_back(f);

		if (state == task_state::FINISHED)
			f();

		return index;
	}

	void remove_handler(size_t index)
	{
		if (index >= on_finished.size())
			throw std::runtime_error("invalid index");

		on_finished[index] = 0;
	}

	bool finish()
	{
		logDebug("task_base::finish()\n");

		std::unique_lock<std::mutex> lock(mutex);

		switch (state) {
		case task_state::CREATED:
			state = task_state::RUNNING;

			// TODO: keep locked while running task?
			lock.unlock();

			if (run()) {
				enter_state(task_state::FINISHED);
				return true;
			}
			break;

		case task_state::FINISHED:
			return true;
		}

		return false;
	}

protected:
	void enter_state(task_state state)
	{
		logDebug("task_base::enter_state(%d)\n", state);
		
		std::unique_lock<std::mutex> lock(mutex);

		if (this->state == state)
			return;

		switch (state) {
		case task_state::FINISHED:
			if (this->state != task_state::RUNNING)
				throw std::runtime_error("not running");

			for (auto f : on_finished) {
				if (f)
					f();
			}
			break;

		default:
			throw std::runtime_error("invalid transition");
		}

		this->state = state;
	}

	virtual bool run() = 0;
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
		logDebug("task::task()\n");
	}

	virtual ~task()
	{
		logDebug("task::~task()\n");
	}

protected:
	virtual bool run()
	{
		logDebug("task::run()...\n");

		if constexpr (std::is_void_v<std::invoke_result_t<Callable,Args...>>)
			std::apply(callable, args);
		else
			value = std::apply(callable, args);

		logDebug("task::run() done.\n");

		return true;
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
		logDebug("joinable::join()\n");

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

class task_node
{
private:
	std::shared_ptr<task_base> task;
	unsigned int wait;
	vthread thread;
	bool managed;
	size_t handler_index;
	bool finished;
	std::mutex lock;
	std::condition_variable cond;

public:
	task_node(std::string name, std::shared_ptr<task_base> task, unsigned int wait, bool managed = true)
		:
		task(task),
		wait(wait),
		thread(name),
		managed(managed),
		finished(false)
	{
		logDebug("task_node::task_node(%p, '%s', %u)\n", this, name.c_str(), wait);

		handler_index = task->handle_finished([this]() {
			std::unique_lock<std::mutex> l(lock);

			finished = true;

			cond.notify_all();
		});
	}

	~task_node()
	{
		logDebug("task_node::~task_node(%p)\n", this);

		task->remove_handler(handler_index);
	}

	void add_to_notify(std::shared_ptr<task_node> node)
	{
		logDebug("task_node::add_to_notify(%p)\n", this);

		task->handle_finished([node]() {
			node->notify();
		});
	}

	void notify()
	{
		logDebug("task_node::notify(%p)\n", this);
		
		std::unique_lock<std::mutex> l(lock);

		if (!--wait) {
			thread.start([this]() {
				bool finished = task->finish();
			}, managed);
		}
	}

	void join()
	{
		logDebug("task_node::join(%p)\n", this);

		std::unique_lock<std::mutex> l(lock);

		while (!finished)
			cond.wait(l);
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

class task_queue
{
private:
	std::shared_ptr<task_node> first;
	std::shared_ptr<task_node> last;
	std::mutex mutex;

public:
	task_queue() {}

	void push(std::shared_ptr<task_base> task, bool reset = true)
	{
		logDebug("task_queue::push()\n");

		std::unique_lock<std::mutex> lock(mutex);

		if (first) {
			auto node = std::make_shared<task_node>("task", task, 1);

			last->add_to_notify(node);

			last = node;
		}
		else {
			auto node = std::make_shared<task_node>("first", task, 1);

			first = node;
			last = node;
		}
	}

	joinable exec(bool detached = false)
	{
		logDebug("task_queue::exec(%s)\n", detached ? "true" : "false");

		std::unique_lock<std::mutex> lock(mutex);

		auto f = first;
		auto l = last;

		first.reset();
		last.reset();

		lock.unlock();

		f->notify();

		if (!detached) {
			l->join();

			return joinable();
		}

		return joinable([f,l]() {
			l->join();
		});
	}
};


// parallel_f :: task_list == implementation

typedef unsigned long long task_id;

class task_list
{
private:
	task_id ids;
	std::map<task_id,std::shared_ptr<task_node>> nodes;
	std::mutex mutex;
	std::shared_ptr<task_node> flush_join;

public:
	task_list()
		:
		ids(0),
		flush_join(0)
	{
		logDebug("task_list::task_list(%p)\n", this);
	}

	~task_list()
	{
		logDebug("task_list::~task_list(%p)\n", this);
	}

	task_id append(std::shared_ptr<task_base> task)		// TODO: reset option?
	{
		logDebug( "task_list::append( no dependencies )\n" );

		std::unique_lock<std::mutex> lock(mutex);

		task_id id = ++ids;

		nodes[id] = std::make_shared<task_node>("task", task, 1);

		return id;
	}

	template <typename ...Deps>
	task_id append(std::shared_ptr<task_base> task, Deps... deps)		// TODO: reset option?
	{
		logDebug( "task_list::append( %d dependencies )\n", sizeof...(deps) );

		std::unique_lock<std::mutex> lock(mutex);

		task_id id = ++ids;

		std::shared_ptr<task_node> node = std::make_shared<task_node>("task", task, 1 + sizeof...(deps));

		std::list<task_id> deps_list({ deps... });

		for (auto li : deps_list) {
			logDebug("task_list::append()  <- id %llu\n", li);

			auto dep = nodes.find(li);

			if (dep != nodes.end())
				(*dep).second->add_to_notify(node);
			else
				node->notify();
		}

		nodes[id] = node;

		return id;
	}

	joinable finish(bool detached = false)
	{
		logDebug( "task_list::finish(%s)\n", detached ? "true" : "false");

		std::unique_lock<std::mutex> lock(mutex);

		for (auto node : nodes)
			node.second->notify();

		if (flush_join) {
			logDebug("task_list::finish() joining previous flush...\n");
			flush_join->join();
			logDebug("task_list::finish() joined previous flush.\n");
			flush_join.reset();
		}

		if (!detached) {
			for (auto node : nodes)
				node.second->join();

			nodes.clear();

			return joinable();
		}

		auto n = nodes;

		nodes.clear();

		return joinable([n]() {
			for (auto node : n)
				node.second->join();
		});
	}

	task_id flush()
	{
		logDebug("task_list::flush()\n");

		std::unique_lock<std::mutex> lock(mutex);

		std::list<std::shared_ptr<task_node>> flush;

		for (auto node : nodes) {
			node.second->notify();

			if (node.second != flush_join)
				flush.push_back(node.second);
		}

		nodes.clear();

		if (flush_join) {
			logDebug("task_list::flush() joining previous flush...\n");
			flush_join->join();
			logDebug("task_list::flush() joined previous flush.\n");
			flush_join.reset();
		}

		auto task = make_task([flush]() {
			for (auto node : flush) {
				node->join();
				node.reset();
			}
		});

		flush_join = std::make_shared<task_node>("flush", task, 1, false);

		task_id flush_id = ++ids;

		nodes[flush_id] = flush_join;

//		flush_join->notify();

		return flush_id;
	}

	size_t length() const
	{
		return nodes.size();
	}
};

}
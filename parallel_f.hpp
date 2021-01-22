// === (C) 2020/2021 === parallel_f (tasks, queues, lists in parallel threads)
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
	task_base()
		:
		state(task_state::CREATED)
	{
		LOG_DEBUG("task_base::task_base(%p)\n", this);
	}

	~task_base()
	{
		LOG_DEBUG("task_base::~task_base(%p)\n", this);
	}

	size_t handle_finished(std::function<void(void)> f)
	{
		LOG_DEBUG("task_base::handle_finished(%p, %s)\n", this, f.target_type().name());

		std::unique_lock<std::mutex> lock(mutex);
		
		size_t index = on_finished.size();

		on_finished.push_back(f);

		if (state == task_state::FINISHED)
			f();

		return index;
	}

	void remove_handler(size_t index)
	{
		LOG_DEBUG("task_base::remove_handler(%p, %zu)\n", this, index);

		std::unique_lock<std::mutex> lock(mutex);

		if (index >= on_finished.size())
			throw std::runtime_error("invalid index");

		on_finished[index] = 0;
	}

	bool finish()
	{
		LOG_DEBUG("task_base::finish(%p)\n", this);

		std::unique_lock<std::mutex> lock(mutex);

		switch (state) {
		case task_state::CREATED:
			state = task_state::RUNNING;

			lock.unlock();

			if (run()) {
				enter_state(task_state::FINISHED);
				LOG_DEBUG("task_base::finish(%p) returning true\n", this);
				return true;
			}
			break;

		case task_state::FINISHED:
			LOG_DEBUG("task_base::finish(%p) returning true\n", this);
			return true;
		}

		LOG_DEBUG("task_base::finish(%p) returning false\n", this);

		return false;
	}

protected:
	void enter_state(task_state state)
	{
		LOG_DEBUG("task_base::enter_state(%p, %d)\n", this, state);
		
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
			LOG_DEBUG("task_info::task_info(): [[%s]], showing %zu arguments...\n", typeid(Callable).name(), sizeof...(args));

			if (getDebugLevel() > 0)
				(DumpArg<Args>(args), ...);
		}
		else
			LOG_DEBUG("task_info::task_info(): [[%s]], no arguments...\n", typeid(Callable).name());
	}

public:
	Value result() { return Value(this->shared_from_this()); }

private:
	template <typename ArgType>
	void DumpArg(ArgType arg)
	{
		LOG_DEBUG("task_info::task_info():   Type: %s\n", typeid(ArgType).name());
	}

	template <>
	void DumpArg(Value arg)
	{
		switch (parallel_f::getDebugLevel()) {
		case 2:
			arg.task->finish();
		case 1:
			LOG_DEBUG("task_info::task_info():   Type: Value( %s )\n", arg.get<std::any>().type().name());
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
		LOG_DEBUG("task::task()\n");
	}

	virtual ~task()
	{
		LOG_DEBUG("task::~task()\n");
	}

protected:
	virtual bool run()
	{
		LOG_DEBUG("task::run()...\n");

		if constexpr (std::is_void_v<std::invoke_result_t<Callable,Args...>>)
			std::apply(callable, args);
		else
			value = std::apply(callable, args);

		LOG_DEBUG("task::run() done.\n");

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

class task_node
{
private:
	std::shared_ptr<task_base> task;
	unsigned int wait;
	bool managed;
	std::shared_ptr<vthread> thread;
	size_t handler_index;
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

		handler_index = task->handle_finished([this]() {
			std::unique_lock<std::mutex> l(lock);

			finished = true;

			cond.notify_all();
		});
	}

	~task_node()
	{
		LOG_DEBUG("task_node::~task_node(%p '%s')\n", this, get_name().c_str());

		task->remove_handler(handler_index);
	}

	void add_to_notify(std::shared_ptr<task_node> node)
	{
		LOG_DEBUG("task_node::add_to_notify(%p '%s', %p)\n", this, get_name().c_str(), node.get());

		task->handle_finished([node]() {
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
		LOG_DEBUG("task_queue::push()\n");

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
		LOG_DEBUG("task_queue::exec(%s)\n", detached ? "true" : "false");

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
		LOG_DEBUG("task_list::task_list(%p)\n", this);
	}

	~task_list()
	{
		LOG_DEBUG("task_list::~task_list(%p)\n", this);
	}

	task_id append(std::shared_ptr<task_base> task)
	{
		LOG_DEBUG("task_list::append( no dependencies )\n");

		std::unique_lock<std::mutex> lock(mutex);

		task_id id = ++ids;

		nodes[id] = std::make_shared<task_node>("task", task, 1);

		return id;
	}

	template <typename ...Deps>
	task_id append(std::shared_ptr<task_base> task, Deps... deps)
	{
		LOG_DEBUG("task_list::append( %d dependencies )\n", sizeof...(deps));

		std::unique_lock<std::mutex> lock(mutex);

		task_id id = ++ids;

		std::shared_ptr<task_node> node = std::make_shared<task_node>("task", task, (unsigned int)(1 + sizeof...(deps)));

		std::list<task_id> deps_list({ deps... });

		for (auto li : deps_list) {
			LOG_DEBUG("task_list::append()  <- id %llu\n", li);

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
		LOG_DEBUG("task_list::finish(%s)\n", detached ? "true" : "false");

		std::unique_lock<std::mutex> lock(mutex);

		if (flush_join) {
			LOG_DEBUG("task_list::finish() joining previous flush...\n");
			flush_join->join();
			LOG_DEBUG("task_list::finish() joined previous flush.\n");
			flush_join.reset();
		}

		for (auto node : nodes)
			node.second->notify();

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
		LOG_DEBUG("task_list::flush()...\n");

		std::unique_lock<std::mutex> lock(mutex);

		std::shared_ptr<task_node> prev_flush_join = flush_join;

		flush_join = std::make_shared<task_node>("flush", make_task([]() {}), (unsigned int)(1 + nodes.size()), false);

		if (prev_flush_join) {
			LOG_DEBUG("task_list::flush() joining previous flush...\n");
			prev_flush_join->join();
			LOG_DEBUG("task_list::flush() joined previous flush.\n");
		}

		LOG_DEBUG("task_list::flush() flushing (notify) nodes...\n");

		for (auto node : nodes) {
			node.second->add_to_notify(flush_join);

			node.second->notify();
		}

		LOG_DEBUG("task_list::flush() clearing nodes...\n");

		nodes.clear();

		LOG_DEBUG("task_list::flush() clearing nodes done.\n");

		task_id flush_id = ++ids;

		nodes[flush_id] = flush_join;

		LOG_DEBUG("task_list::flush() done.\n");

		return flush_id;
	}

	size_t length() const
	{
		return nodes.size();
	}
};

}
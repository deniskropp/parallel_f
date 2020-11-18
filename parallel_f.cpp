// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <any>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <tuple>


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


// parallel_f :: task_queue == implementation

class joinable
{
	friend class task_queue;

private:
	std::thread* thread;

	joinable(std::thread* thread = NULL)
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

class task_queue
{
private:
	std::queue<task_base*> tasks;
	std::mutex mutex;

public:
	task_queue() {}

	void push(task_base* task, bool reset = true)
	{
		std::unique_lock<std::mutex> lock(mutex);

		if (reset)
			task->reset();

		tasks.push(task);
	}

	joinable exec(bool detached = false)
	{
		std::unique_lock<std::mutex> lock(mutex);

		std::queue<task_base*> fq = tasks;

		tasks = std::queue<task_base*>();

		lock.unlock();

		if (detached) {
			std::thread* t = new std::thread([fq]() {
					finish_all(fq);
				});

			return joinable(t);
		}

		finish_all(fq);

		return joinable();
	}

private:
	static void finish_all(std::queue<task_base*> fq)
	{
		while (!fq.empty()) {
			auto task = fq.front();

			fq.pop();

			task->finish();
		}
	}
};


// parallel_f :: task == implementation

template <typename Callable, typename... Args>
class task : public task_base
{
private:
	Callable callable;
	std::tuple<Args...> args;
	std::any value;

public:
	task(Callable& callable, Args&... args)
		:
		callable(callable),
		args(std::make_tuple(args...))
	{
	}

	const std::any& result() const { return value; }

protected:
	virtual void run()
	{
		value = std::apply(callable, args);		// TODO: add std::any return value for tasks?
	}
};

template <typename Callable, typename... Args>
auto make_task(Callable callable, Args... args)
{
	return task<Callable, Args...>(callable, args...);
}

}



// parallel_f :: task_queue == testing example

int main()
{
	auto func1 = [](auto a) -> std::string
	{
		std::stringstream str;

		str << "===>> func1 " << a << std::endl;

		std::cout << str.str();

		return "f1_result";
	};

	auto func2 = [](auto a, auto b, auto t1)
	{
		std::stringstream str;

		str << "===>> func2 " << a << "  " << b << "  (" << std::any_cast<std::string>(t1->result()) << ")" << std::endl;

		std::cout << str.str();

		return std::any();
	};

	auto task1 = parallel_f::make_task(func1, "running task1...");
	auto task2 = parallel_f::make_task(func2, "running task2...", 123, &task1);
	auto task3 = parallel_f::make_task(func2, "running task3...", 456, &task1);

	// round 1 = initial test with three tasks
	parallel_f::task_queue tq;

	tq.push(&task1);
	tq.push(&task2);
	tq.push(&task3);

	auto joinable = tq.exec(true);

	joinable.join();


	// round 2 = push (reset) two of our tasks once again
	tq.push(&task1);
	tq.push(&task2);

	joinable = tq.exec(true);

	joinable.join();


	// round 3 = like round 2 with second queue and new task
	tq.push(&task1);
	tq.push(&task2);

	auto task4 = parallel_f::make_task(func2, "running task4...", 7890, &task1);

	parallel_f::task_queue queue2;

	queue2.push(&task3);
	queue2.push(&task4);

//	tq.push(queue2);	<== like pushing another queue(2) instead of tasks to our first queue
	auto queue2_task = parallel_f::make_task([&queue2]() {
			queue2.exec();
			return std::any();
		});
	tq.push(&queue2_task);	// TODO: move these (previous) four lines into task_queue::push(task_queue&)???

	joinable = tq.exec(true);

	joinable.join();


	void test_list();
	test_list();
}


// parallel_f :: task_list == implementation

namespace parallel_f {

typedef unsigned long long task_id;

class task_list
{
private:
	task_id ids;
	std::map<task_id, std::pair<task_base*, std::list<task_id>>> tasks;
	std::mutex mutex;

public:
	task_list() : ids(0) {}

	task_id append(task_base* task)		// TODO: reset option?
	{
		std::cout << "task_list::append( no dependencies )" << std::endl;

		std::unique_lock<std::mutex> lock(mutex);

		task_id id = ++ids;

		tasks[id] = std::make_pair(task, std::list<task_id>());

		return id;
	}

	template <typename Task = task_base*, typename ...Deps>
	task_id append(task_base* task, Deps... deps)		// TODO: reset option?
	{
		std::cout << "task_list::append( " << sizeof...(deps) << " dependencies )" << std::endl;

		std::unique_lock<std::mutex> lock(mutex);

		task_id id = ++ids;

		std::list<task_id> deps_list({ deps... });

		for (auto li : deps_list)
			std::cout << "  <- id " << li << std::endl;

		tasks[id] = std::make_pair(task, deps_list);

		return id;
	}

	void finish(task_id id = 0)		// TODO: detach option?
	{
		std::cout << "task_list::finish( id " << id << " )" << std::endl;

		std::unique_lock<std::mutex> lock(mutex);

		if (id != 0) {
			auto found = tasks.find(id);

			if (found == tasks.end())
				return;

			lock.unlock();

			std::list<joinable> joinables;

			for (auto li : found->second.second) {
				std::cout << "  -> id " << li << std::endl;

				// we are using make_task and task_queue.exec() detaching,
				// joining before running the actual task
				//		finish(li);

				auto task = make_task([li,this]() {
						finish(li);
						return std::any();
					});

				task_queue tq;

				tq.push(&task);

				joinables.push_back(tq.exec(true));
			}

			for (auto j : joinables)
				j.join();

			found->second.first->finish();
		}
		else {
			for (auto task : tasks) {
				lock.unlock();

				task.second.first->finish();

				lock.lock();
			}
		}
	}
};

}


// parallel_f :: task_list == testing example

void test_list()
{
	auto func = [](auto a)
	{
		std::stringstream str;

		str << "===>> func " << a << std::endl;

		std::cout << str.str();

		return std::any();
	};

	auto task1 = parallel_f::make_task(func, "running task1...");
	auto task2 = parallel_f::make_task(func, "running task2...");
	auto task3 = parallel_f::make_task(func, "running task3...");
	auto task4 = parallel_f::make_task(func, "running task4...");
	auto task5 = parallel_f::make_task(func, "running task5...");


	parallel_f::task_list tl;

	auto a1_id = tl.append(&task1);
	auto a2_id = tl.append(&task2,a1_id);
	auto a3_id = tl.append(&task3,a1_id);
	auto a4_id = tl.append(&task4,a2_id,a3_id);
	auto a5_id = tl.append(&task5,a4_id);

//	tl.finish(a1_id);
//	tl.finish(a2_id);
//	tl.finish(a3_id);
//	tl.finish();
	tl.finish(a5_id);
}
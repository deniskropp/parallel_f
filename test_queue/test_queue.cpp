// === (C) 2020 === parallel_f / test_queue (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


// parallel_f :: task_queue == testing example

int main()
{
	parallel_f::setDebugLevel(0);


	auto func1 = []() -> std::string
	{
		parallel_f::logInfo("First function being called\n");

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		return "Hello World";
	};

	auto func2 = [](auto msg) -> std::string
	{
		parallel_f::logInfo("Second function receiving '%s'\n", msg.get<std::string>().c_str());

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		return "Good bye";
	};

	auto func3 = [](auto msg) -> std::string
	{
		parallel_f::logInfo("Third function receiving '%s'\n", msg.get<std::string>().c_str());

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		return "End";
	};

	parallel_f::joinables j;
	parallel_f::task_queue tq;

	for (int i = 0; i < 3; i++) {
		// round 1 = initial test with three tasks
		auto task1 = parallel_f::make_task(func1);
		auto task2 = parallel_f::make_task(func2, task1->result());
		auto task3 = parallel_f::make_task(func3, task2->result());

		tq.push(task1);
		tq.push(task2);
		tq.push(task3);

		j.add(tq.exec(true));
	}

	for (int i = 0; i < 3; i++) {
		// round 2 = initial test with three tasks
		auto task1 = parallel_f::make_task(func1);
		auto task2 = parallel_f::make_task(func3, task1->result());
		auto task3 = parallel_f::make_task(func3, task2->result());

		tq.push(task1);
		tq.push(task2);
		tq.push(task3);

		j.add(tq.exec(true));
	}

	for (int i = 0; i < 3; i++) {
		auto task1 = parallel_f::make_task(func1);
		auto task2 = parallel_f::make_task(func2, task1->result());
		auto task3 = parallel_f::make_task(func3, task2->result());

		// round 3 = mixture of past and new tasks
		tq.push(task1);

		parallel_f::task_queue queue2;

		queue2.push(task2);
		queue2.push(task3);

		//	tq.push(queue2);	<== like pushing another queue(2) instead of tasks to our first queue
		auto queue2_task = parallel_f::make_task([&queue2]() {
			parallel_f::logInfo("Special function running whole queue...\n");
			return queue2.exec();
			});
		tq.push(queue2_task);	// TODO: move these (previous) four lines into task_queue::push(task_queue&)???

		j.add(tq.exec(true));
	}

	j.join_all();


	parallel_f::stats::instance::get().show_stats();
}

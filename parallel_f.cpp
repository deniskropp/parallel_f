// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


// parallel_f :: task_queue == testing example

int main()
{
	auto func1 = [](auto a) -> std::string
	{
		std::stringstream str;

		str << "===>> func1 " << a << std::endl;

		std::cout << str.str();

		return "func1 result...";
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
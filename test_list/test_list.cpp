// === (C) 2020 === parallel_f / test_list (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


// parallel_f :: task_list == testing example

int main()
{
	parallel_f::setDebugLevel(0);


	auto func = [](auto a)
	{
		parallel_f::logInfo("Function %s\n", a);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		parallel_f::logInfo("Function %s done.\n", a);

		return parallel_f::none;
	};

	auto task1 = parallel_f::make_task(func, "running task1...");
	auto task2 = parallel_f::make_task(func, "running task2...");
	auto task3 = parallel_f::make_task(func, "running task3...");
	auto task4 = parallel_f::make_task(func, "running task4...");
	auto task5 = parallel_f::make_task(func, "running task5...");
	auto task6 = parallel_f::make_task(func, "running task6...");
	auto task7 = parallel_f::make_task(func, "running task7...");
	auto task8 = parallel_f::make_task(func, "running task8...");
	auto task9 = parallel_f::make_task(func, "running task9...");


	parallel_f::task_list tl;

	auto a1_id = tl.append(task1);
	auto a2_id = tl.append(task2);
	auto a3_id = tl.append(task3, a1_id, a2_id);
	auto a4_id = tl.append(task4);
	auto a5_id = tl.append(task5);
	auto a6_id = tl.append(task6, a4_id, a5_id);
	auto a7_id = tl.append(task7, a3_id, a6_id);
	auto a8_id = tl.append(task8, a7_id);
	auto a9_id = tl.append(task9, a7_id);

	tl.finish();


	parallel_f::stats::instance::get().show_stats();
}

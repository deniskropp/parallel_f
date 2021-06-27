// === (C) 2020/2021 === parallel_f / test_list (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


// parallel_f :: task_list == testing example

int main()
{
	parallel_f::setDebugLevel(0);


	auto func = [](auto a) {
		parallel_f::logInfo("Function %s\n", a.c_str());

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		parallel_f::logInfo("Function %s done.\n", a.c_str());
	};


	std::vector<std::shared_ptr<parallel_f::task_base>> tasks;

	for (int i=0; i<17; i++)
		tasks.push_back(parallel_f::make_task(func, std::string("running task") + std::to_string(i) + "..."));


	parallel_f::task_list tl;

	auto a1_id = tl.append(tasks[0]);
	auto a2_id = tl.append(tasks[1]);
	auto a3_id = tl.append(tasks[2]);
	auto a4_id = tl.append(tasks[3]);
	auto a5_id = tl.append(tasks[4], a1_id, a2_id);
	auto a6_id = tl.append(tasks[5], a2_id, a3_id);
	auto a7_id = tl.append(tasks[6], a3_id, a4_id);
	auto a8_id = tl.append(tasks[7], a5_id);
	auto a9_id = tl.append(tasks[8], a6_id);
	auto a10_id = tl.append(tasks[9], a7_id);
	auto a11_id = tl.append(tasks[10], a8_id, a9_id, a10_id);
	auto a12_id = tl.append(tasks[11], a8_id, a9_id, a10_id);
	auto a13_id = tl.append(tasks[12], a8_id, a9_id, a10_id);
	auto a14_id = tl.append(tasks[13], a8_id, a9_id, a10_id);
	auto a15_id = tl.append(tasks[14], a8_id);
	auto a16_id = tl.append(tasks[15], a9_id);
	auto a17_id = tl.append(tasks[16], a10_id);

	tl.finish();


	parallel_f::stats::instance::get().show_stats();
}

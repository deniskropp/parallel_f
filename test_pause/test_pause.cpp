// === (C) 2020 === parallel_f / test_pause (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


class TestTask : public parallel_f::task_base
{
private:
	std::thread *thread;

public:
	TestTask()
		:
		thread(0)
	{
	}

protected:
	virtual bool run()
	{
		parallel_f::logInfo("TestTask::run()...\n");

		thread = new std::thread([this]() {
			parallel_f::logInfo("  <- TestTask::run() thread...\n");

			std::this_thread::sleep_for(std::chrono::milliseconds(500));

			enter_state(task_state::FINISHED);

			parallel_f::logInfo("  <- TestTask::run() thread done.\n");
		});

		parallel_f::logInfo("TestTask::run() done.\n");

		return false;
	}
};


// parallel_f :: task_list == testing example

int main()
{
	parallel_f::setDebugLevel(0);


	std::vector<std::shared_ptr<parallel_f::task_base>> tasks;

	for (int i = 0; i < 17; i++)
		tasks.push_back(std::make_shared<TestTask>());


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

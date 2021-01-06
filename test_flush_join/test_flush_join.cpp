// === (C) 2020 === parallel_f / test_flush_join (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


static void test_flush();
static void test_join_exec();


int main()
{
	parallel_f::setDebugLevel(0);
	parallel_f::setDebugLevel("task::", 1);
	parallel_f::setDebugLevel("task_list::", 1);
	parallel_f::setDebugLevel("task_queue::", 1);
	parallel_f::setDebugLevel("task_node::", 1);

//	parallel_f::system::instance().setAutoFlush(parallel_f::system::AutoFlush::EndOfLine);
//	parallel_f::system::instance().startFlushThread(10);


	test_flush();
	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();

	test_join_exec();
	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();
}


// parallel_f :: task_list == testing example for task_list::flush()

static void test_flush()
{
	parallel_f::task_list tl;

	parallel_f::task_id flush_id = 0;

	for (int i = 0; i < 10; i++) {
		auto task = parallel_f::make_task([]() {
			parallel_f::logInfo("### task running...\n");

			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			parallel_f::logInfo("### task ending...\n");
		});

		tl.append(task, flush_id);

		//tl.flush(); // using this line instead of the line below (or not passing flush_id in above line) increases parallelism
		flush_id = tl.flush();
	}

	tl.finish();
}



// parallel_f :: task_queue == testing example for task_queue::exec() with detach/join

static void test_join_exec()
{
	parallel_f::task_queue tq;
	parallel_f::joinable j;

	for (int i = 0; i < 10; i++) {
		auto task = parallel_f::make_task([]() {
			parallel_f::logInfo("### task running...\n");

			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			parallel_f::logInfo("### task ending...\n");
		});

		tq.push(task);


		j.join();

		j = tq.exec(true);
	}

	j.join();
}

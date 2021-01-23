// === (C) 2020/2021 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"

int main()
{
	parallel_f::system::instance().setAutoFlush(parallel_f::system::AutoFlush::EndOfLine);

	for (int n = 0; n < 100; n++) {
		parallel_f::task_queue tq;

		for (int i = 0; i < 100; i++)
			tq.push(parallel_f::make_task([]() {}));

		tq.exec();
	}

	parallel_f::stats::instance().show_stats();
	parallel_f::system::instance().flush();
}
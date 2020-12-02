// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


int main()
{
	auto task = parallel_f::make_task([]() {
			std::cout << "Hello World" << std::endl;
		});

	parallel_f::task_list tl;

	tl.append(task);
	tl.finish();

	parallel_f::stats::instance::get().show_stats();
}

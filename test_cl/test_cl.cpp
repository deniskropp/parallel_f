// === (C) 2020 === parallel_f / test_cl (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <vector>

#include "parallel_f.hpp"
#include "task_cl.hpp"



static void test_cl_queue();
static void test_cl_list();
//static void test_cl_objects();


int main()
{
	parallel_f::setDebugLevel(0);
	parallel_f::system::instance().setAutoFlush(parallel_f::system::AutoFlush::EndOfLine);


	test_cl_queue();
	parallel_f::stats::instance::get().show_stats();

//	test_cl_list();
//	parallel_f::stats::instance::get().show_stats();

	//	test_cl_objects();
	//	parallel_f::stats::instance::get().show_stats();
}


// parallel_f :: task_cl == testing OpenCL kernel execution (using task queue)

static void test_cl_queue()
{
	std::vector<std::byte> data(16 * 1024 * 1024);

	task_cl::task_cl_kernel_args args(new task_cl::task_cl_kernel_args::kernel_arg_mem(data.size(), data.data(), NULL),
									  new task_cl::task_cl_kernel_args::kernel_arg_mem(data.size(), NULL, data.data()),
									  new task_cl::task_cl_kernel_args::kernel_arg_t<cl_int>((cl_int)data.size() / 16));

	auto task_pre = task_cl::task_cl_kernel_pre::make_task(args);
	auto task_post = task_cl::task_cl_kernel_post::make_task(args);

	parallel_f::task_queue tq;
	parallel_f::joinables j;

	tq.push(task_pre);
	tq.exec();

	auto kernel = task_cl::make_kernel("cltest.cl", "CLTest2", data.size() / 16, 256);

	for (int i = 0; i < 3; i++) {
		auto task_exec = task_cl::task_cl_kernel_exec::make_task(args, kernel);

		tq.push(task_exec);

		j.add(tq.exec(true));
	}

	j.join_all();

	tq.push(task_post);
	tq.exec();
}


// parallel_f :: task_cl == testing OpenCL kernel execution (using task list)

static void test_cl_list()
{
	std::vector<std::byte> src(16 * 1024 * 1024), dst(src.size());

	task_cl::task_cl_kernel_args kernel_args(new task_cl::task_cl_kernel_args::kernel_arg_mem(src.size(), src.data(), NULL),
											 new task_cl::task_cl_kernel_args::kernel_arg_mem(dst.size(), NULL, dst.data()),
											 new task_cl::task_cl_kernel_args::kernel_arg_t<cl_int>((cl_int)src.size() / 16));

	parallel_f::task_list tl;

	for (int i = 0; i < 3; i++) {
		auto task = task_cl::make_task("cltest.cl", "CLTest2", src.size() / 16, 256, kernel_args);

		tl.append(task);
	}

	tl.finish();
}

/*
static void test_cl_objects()
{
	class object
	{
	public:
		int x;
		int y;
		int seq;
	};

	std::vector<object> objects(1000);

	parallel_f::task_list tl;
	parallel_f::task_id flush_id = 0;

	task_cl::task_cl_kernel_args kernel_args(new task_cl::task_cl_kernel_args::kernel_arg_mem(objects.size() * sizeof(object), objects.data(), objects.data()),
											 new task_cl::task_cl_kernel_args::kernel_arg_t<cl_int>((cl_int)objects.size()));

	for (int i = 0; i < 20; i++) {
		auto task = task_cl::make_task("cltest.cl", "RunObjects", objects.size(), 100, kernel_args);

		auto run_id = tl.append(task, flush_id);

		auto log_task = parallel_f::make_task([&objects]() {
			parallel_f::logInfo("log_task::run()...\n");

			parallel_f::logInfo("object seq %d\n", objects[0].seq);

			parallel_f::logInfo("log_task::run() done.\n");

			return parallel_f::none;
		});

		tl.append(log_task, run_id);

//		flush_id = tl.flush();
		tl.finish();
	}

//	tl.finish();
}
*/
// === (C) 2020 === parallel_f / test_cl (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <vector>

#include "parallel_f.hpp"
#include "task_cl.hpp"



static void test_cl_queue();
static void test_cl_list();
static void test_cl_objects_simple();
static void test_cl_objects_queue();


int main()
{
	parallel_f::setDebugLevel(0);
//	parallel_f::setDebugLevel("task::", 1);
//	parallel_f::setDebugLevel("task_list::", 1);
//	parallel_f::setDebugLevel("task_queue::", 1);
//	parallel_f::setDebugLevel("task_node::", 1);
//	parallel_f::setDebugLevel("task_cl::cl_task::", 1);
	parallel_f::setDebugLevel("task_cl::kernel_pre::", 1);
	parallel_f::setDebugLevel("task_cl::kernel_exec::", 1);
	parallel_f::setDebugLevel("task_cl::kernel_exec::run() <= Queue FINISHED", 1);
	parallel_f::setDebugLevel("task_cl::kernel_post::", 1);

//	parallel_f::system::instance().setAutoFlush(parallel_f::system::AutoFlush::EndOfLine);
//	parallel_f::system::instance().startFlushThread(10);


	for (int i = 0; i < 3; i++) {
		test_cl_queue();
		parallel_f::stats::instance::get().show_stats();
		parallel_f::system::instance().flush();
	}

	for (int i = 0; i < 3; i++) {
		test_cl_list();
		parallel_f::stats::instance::get().show_stats();
		parallel_f::system::instance().flush();
	}

	test_cl_objects_simple();
	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();

	test_cl_objects_queue();
	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();
}


// parallel_f :: task_cl == testing OpenCL kernel execution (using task queue)

static void test_cl_queue()
{
	std::vector<std::byte> data(16 * 1024 * 1024);

	auto args = task_cl::make_args(new task_cl::kernel_args::kernel_arg_mem(data.size(), data.data(), NULL),
								   new task_cl::kernel_args::kernel_arg_mem(data.size(), NULL, data.data()),
								   new task_cl::kernel_args::kernel_arg_t<cl_int>((cl_int)data.size() / 16));

	auto task_pre = task_cl::kernel_pre::make_task(args);
	auto task_post = task_cl::kernel_post::make_task(args);

	parallel_f::task_queue tq;
	parallel_f::joinables j;

	tq.push(task_pre);
	tq.exec();

	auto kernel = task_cl::make_kernel("cltest.cl", "CLTest2", data.size() / 16, 256);

	for (int i = 0; i < 4; i++) {
		auto task_exec = task_cl::kernel_exec::make_task(args, kernel);

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

	parallel_f::task_list tl;

	auto kernel = task_cl::make_kernel("cltest.cl", "CLTest2", src.size() / 16, 256);

	for (int i = 0; i < 4; i++) {
		auto args = task_cl::make_args(new task_cl::kernel_args::kernel_arg_mem(src.size(), src.data(), NULL),
									   new task_cl::kernel_args::kernel_arg_mem(dst.size(), NULL, dst.data()),
									   new task_cl::kernel_args::kernel_arg_t<cl_int>((cl_int)src.size() / 16));

		auto task = task_cl::make_task(kernel, args);

		tl.append(task);

		tl.flush();
	}

	tl.finish();
}


// parallel_f :: task_cl == testing OpenCL kernel execution (with objects)

static void test_cl_objects_simple()
{
	class object
	{
	public:
		int x;
		int y;
		int seq;

		object()
			:
			x(0),
			y(0),
			seq(0)
		{
		}
	};

	std::vector<object> objects(1000);

	parallel_f::task_list tl;
	parallel_f::task_id flush_id = 0;

	auto kernel = task_cl::make_kernel("cltest.cl", "RunObjects", objects.size(), 100);

	for (int i = 0; i < 20; i++) {
		auto args = task_cl::make_args(new task_cl::kernel_args::kernel_arg_mem(objects.size() * sizeof(object), objects.data(), objects.data()),
									   new task_cl::kernel_args::kernel_arg_t<cl_int>((cl_int)objects.size()));

		auto task = task_cl::make_task(kernel, args);

		auto task_id = tl.append(task, flush_id);


		auto task_log = parallel_f::make_task([&objects]() {
			parallel_f::logInfo("object seq %d\n", objects[0].seq);
		});

		tl.append(task_log, task_id);

		
		flush_id = tl.flush();
	}

	tl.finish();
}


// parallel_f :: task_cl == testing OpenCL kernel execution (with objects)

static void test_cl_objects_queue()
{
	class object
	{
	public:
		int x;
		int y;
		int seq;

		object()
			:
			x(0),
			y(0),
			seq(0)
		{
		}
	};

	std::vector<object> objects(1000);

	parallel_f::task_queue tq;
	parallel_f::joinable j;

	auto kernel = task_cl::make_kernel("cltest.cl", "RunObjects", objects.size(), 100);

	for (int i = 0; i < 20; i++) {
		auto args = task_cl::make_args(new task_cl::kernel_args::kernel_arg_mem(objects.size() * sizeof(object), objects.data(), objects.data()),
									   new task_cl::kernel_args::kernel_arg_t<cl_int>((cl_int)objects.size()));

		auto task_pre = task_cl::kernel_pre::make_task(args);
		auto task_exec = task_cl::kernel_exec::make_task(args, kernel);
		auto task_post = task_cl::kernel_post::make_task(args);

		tq.push(task_pre);
		tq.push(task_exec);
		tq.push(task_post);


		auto task_log = parallel_f::make_task([&objects]() {
			parallel_f::logInfo("object seq %d\n", objects[0].seq);
		});

		tq.push(task_log);


		j.join();

		j = tq.exec(true);
	}

	j.join();
}

// === (C) 2020/2021 === parallel_f / test_cl (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <sstream>
#include <vector>

#include "parallel_f.hpp"
#include "task_cl.hpp"
#include "test_utils.hpp"



static void test_cl_init();

static void test_cl_codegen();

<<<<<<< HEAD
=======
template <typename Tq = parallel_f::task_queue>
static void test_cl_bench_latency();

template <typename Tq = parallel_f::task_queue>
static void test_cl_bench_throughput();

>>>>>>> e37acf9dbe2063ff1e7561c2c2cb5f3f436d3db4
static void test_cl_queue();
static void test_cl_list();
static void test_cl_objects_simple();
static void test_cl_objects_queue();

template <typename Tq = parallel_f::task_queue>
static void test_cl_bench_latency();

template <typename Tq = parallel_f::task_queue>
static void test_cl_bench_throughput();


int main()
{
	parallel_f::setDebugLevel(0);
//	parallel_f::setDebugLevel("task::", 1);
//	parallel_f::setDebugLevel("task_list::", 1);
//	parallel_f::setDebugLevel("task_queue::", 1);
//	parallel_f::setDebugLevel("task_node::", 1);
//	parallel_f::setDebugLevel("task_cl::cl_task::", 1);
//	parallel_f::setDebugLevel("task_cl::kernel_pre::", 1);
//	parallel_f::setDebugLevel("task_cl::kernel_exec::", 1);
//	parallel_f::setDebugLevel("task_cl::kernel_exec::run() <= Queue FINISHED", 1);
//	parallel_f::setDebugLevel("task_cl::kernel_post::", 1);

//	parallel_f::system::instance().setAutoFlush(parallel_f::system::AutoFlush::EndOfLine);
//	parallel_f::system::instance().startFlushThread(10);

	RUN(test_cl_init());

<<<<<<< HEAD
	RUN(test_cl_codegen());

	RUN(test_cl_bench_latency<parallel_f::task_queue>());

	RUN(test_cl_bench_throughput<parallel_f::task_queue>());
=======
	test_cl_bench_latency<parallel_f::task_queue>();
	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();

	test_cl_bench_throughput<parallel_f::task_queue>();
	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();

	test_cl_bench_latency<parallel_f::task_queue_simple>();
	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();

	test_cl_bench_throughput<parallel_f::task_queue_simple>();
	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();

	test_cl_codegen();
	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();
>>>>>>> e37acf9dbe2063ff1e7561c2c2cb5f3f436d3db4

	for (int i = 0; i < 3; i++)
		RUN(test_cl_queue());

	for (int i = 0; i < 3; i++)
		RUN(test_cl_list());

	RUN(test_cl_objects_simple());

	RUN(test_cl_objects_queue());
}


// parallel_f :: task_cl == minimal code to explicitly run setup (done once initially)

static void test_cl_init()
{
	task_cl::system::instance();
}


// parallel_f :: task_cl == testing OpenCL kernel execution (from genrated code)

static void test_cl_codegen()
{
	std::stringstream ss;

	ss << "int sum(int a, int b) { return a + b; }\n";

	ss << "__kernel void sums(__global int *aa, __global int *bb, __global int *cc, int n)\n";
	ss << "{\n";
	ss << "    int idx = get_global_id(0);\n";
	ss << "\n";
	ss << "    if (idx < n)\n";
	ss << "        cc[idx] = sum(aa[idx], bb[idx]);\n";
	ss << "}\n";

	std::vector<int> aa(1024);
	std::vector<int> bb(1024);
	std::vector<int> cc(1024);

	for (int i = 0; i < 1024; i++) {
		aa[i] = i * 123;
		bb[i] = i * 345;
	}

	auto kernel = task_cl::make_kernel_from_source(ss.str(), "sums", aa.size(), 256);

	auto args = task_cl::make_args(new task_cl::kernel_args::kernel_arg_mem(aa.size(), aa.data(), NULL),
								   new task_cl::kernel_args::kernel_arg_mem(bb.size(), bb.data(), NULL),
								   new task_cl::kernel_args::kernel_arg_mem(cc.size(), NULL, cc.data()),
								   new task_cl::kernel_args::kernel_arg_t<cl_int>((cl_int)aa.size()));

	auto task_pre = task_cl::kernel_pre::make_task(args);
	auto task_exec = task_cl::kernel_exec::make_task(args, kernel);
	auto task_post = task_cl::kernel_post::make_task(args);

	parallel_f::task_queue tq;

	tq.push(task_pre);
	tq.push(task_exec);
	tq.push(task_post);
	tq.exec();
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


// parallel_f :: task_cl == testing OpenCL kernel execution performance (latency)

template <typename Tq>
static void test_cl_bench_latency()
{
	Tq tq;

	auto kernel = task_cl::make_kernel("cltest.cl", "TestBench", 1, 1);

	auto args = task_cl::make_args(new task_cl::kernel_args::kernel_arg_t<cl_uint>(1));

	parallel_f::sysclock clock;

	for (int i = 0; i < 10; i++) {
		auto task = task_cl::kernel_exec::make_task(args, kernel);
//		auto task = parallel_f::make_task([](unsigned int n) { for (volatile unsigned int i = 0; i < n;  i++); }, 1);

		tq.push(task);

		clock.reset();

		tq.exec();

		float latency = clock.reset();

		parallel_f::logInfo("Kernel Execution Latency: %f seconds\n", latency);
	}
}


// parallel_f :: task_cl == testing OpenCL kernel execution performance (throughput)

template <typename Tq>
static void test_cl_bench_throughput()
{
	Tq tq;

	auto kernel = task_cl::make_kernel("cltest.cl", "TestBench", 1, 1);

	auto args = task_cl::make_args(new task_cl::kernel_args::kernel_arg_t<cl_uint>(1));

	parallel_f::sysclock clock;

	for (int n = 0; n < 10; n++) {
		for (int i = 0; i < 20; i++) {
			auto task = task_cl::kernel_exec::make_task(args, kernel);
//			auto task = parallel_f::make_task([](unsigned int n) { for (volatile unsigned int i = 0; i < n;  i++); }, 1);

			tq.push(task);
		}

		clock.reset();

		tq.exec();

		float duration = clock.reset();

		parallel_f::logInfo("Kernel Execution Throughput: %f per second\n", 20.0f / duration);
	}
}

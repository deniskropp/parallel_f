// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <fstream>

#include "parallel_f.hpp"
#include "task_cl.hpp"



static void test_queue();
static void test_list();
static void test_extract_todos(std::string initial_file);
static void test_nested_tasking();
static void test_cl_kernel();
static void test_cl2();
static void test_cl_objects();
static void test_objects();


int main()
{
	parallel_f::setDebugLevel(0);

	test_queue();
	parallel_f::stats::instance::get().show_stats();

	test_list();
	parallel_f::stats::instance::get().show_stats();

//	test_extract_todos("parallel_f.cpp");

//	test_nested_tasking();

//	test_cl_kernel();

//	test_cl2();

	test_objects();
	parallel_f::stats::instance::get().show_stats();
}


// parallel_f :: task_queue == testing example

static void test_queue()
{
	auto func1 = []() -> std::string
	{
		parallel_f::logInfo("First function being called\n");

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		return "Hello World";
	};

	auto func2 = [](auto msg) -> std::string
	{
		parallel_f::logInfo("Second function receiving '%s'\n", msg.get<std::string>().c_str());

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		return "Good bye";
	};

	auto func3 = [](auto msg) -> std::string
	{
		parallel_f::logInfo("Third function receiving '%s'\n", msg.get<std::string>().c_str());

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		return "End";
	};

	parallel_f::joinables j;
	parallel_f::task_queue tq;

	{
		// round 1 = initial test with three tasks
		auto task1 = parallel_f::make_task(func1);
		auto task2 = parallel_f::make_task(func2, task1->result());
		auto task3 = parallel_f::make_task(func3, task2->result());

		tq.push(task1);
		tq.push(task2);
		tq.push(task3);

		j.add(tq.exec(true));
	}

	{
		// round 2 = initial test with three tasks
		auto task1 = parallel_f::make_task(func1);
		auto task2 = parallel_f::make_task(func3, task1->result());
		auto task3 = parallel_f::make_task(func3, task2->result());

		tq.push(task1);
		tq.push(task2);
		tq.push(task3);

		j.add(tq.exec(true));
	}

	{
		auto task1 = parallel_f::make_task(func1);
		auto task2 = parallel_f::make_task(func2, task1->result());
		auto task3 = parallel_f::make_task(func3, task2->result());

		// round 3 = mixture of past and new tasks
		tq.push(task1);

		parallel_f::task_queue queue2;

		queue2.push(task2);
		queue2.push(task3);

		//	tq.push(queue2);	<== like pushing another queue(2) instead of tasks to our first queue
		auto queue2_task = parallel_f::make_task([&queue2]() {
				parallel_f::logInfo("Special function running whole queue...\n");
				return queue2.exec();
			});
		tq.push(queue2_task);	// TODO: move these (previous) four lines into task_queue::push(task_queue&)???

		j.add(tq.exec(true));
	}

	j.join_all();
}


// parallel_f :: task_list == testing example

static void test_list()
{
	auto func = [](auto a)
	{
		parallel_f::logInfo("Function %s\n", a);

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

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
}


// parallel_f :: task_list/queue == testing example that extracts all todos from sources

static void test_extract_todos(std::string initial_file)
{
	auto readfile_func = [](auto filename)
	{
		parallel_f::logDebug( "  <= ::: => reading source code (file path) %s\n", filename.c_str() );

		std::ifstream filestream(filename);
		std::string   filestring;

		while (!filestream.eof()) {
			char str[1024];

			filestream.getline(str, 1024);

			filestring.append(str);
			filestring.append("\n");
		}

		return filestring;
	};

	auto parsestring_func = [](auto t1)
	{
		std::string			   filestring = t1.get<std::string>();
		std::list<std::string> included_files;

		size_t pos;

		for (pos = filestring.find("#include \""); pos < filestring.size(); pos = filestring.find("#include \"", pos + 1)) {
			size_t inc_pos = pos + 10;
			size_t inc_end = filestring.find("\"", inc_pos);

			std::string inc_filename = filestring.substr(inc_pos, inc_end - inc_pos);

			parallel_f::logDebug( "     ->> local include at position (file offset) %d <- %s\n", pos, inc_filename.c_str() );

			included_files.push_back(inc_filename);
		}

		return included_files;
	};


	std::map<std::string,bool> mainfiles;

	auto loop_func = [&mainfiles](auto t1)
	{
		std::list<std::string> included_files = t1.get<std::list<std::string>>();

		for (auto& li : included_files) {
			auto it = mainfiles.find(li);

			if (it == mainfiles.end())
				mainfiles.insert(std::make_pair(li, false));
		}

		return std::any();
	};


	mainfiles.insert(std::make_pair(initial_file, false));

	while (!mainfiles.empty()) {
		std::string mainfile;

		for (auto it : mainfiles) {
			if (!it.second) {
				mainfile = it.first;
				mainfiles[mainfile] = true;
				break;
			}
		}

		if (mainfile.empty())
			break;


		parallel_f::task_list tl;
		parallel_f::task_id   id;


		auto maintask = parallel_f::make_task(readfile_func, mainfile);

		id = tl.append(maintask);


		auto parsetask = parallel_f::make_task(parsestring_func, maintask->result());

		id = tl.append(parsetask, id);


		auto looptask = parallel_f::make_task(loop_func, parsetask->result());

		id = tl.append(looptask, id);


		tl.finish();
	}
}


// parallel_f :: task_cl == testing OpenCL kernel execution

static void test_cl_kernel()
{
	void* data = malloc(1024*1024);

	task_cl::task_cl_kernel_args args(new task_cl::task_cl_kernel_args::kernel_arg_mem(1024 * 1024, data, NULL),
									  new task_cl::task_cl_kernel_args::kernel_arg_mem(1024 * 1024, NULL, data),
									  new task_cl::task_cl_kernel_args::kernel_arg_t<cl_int>(1024 * 1024 / 16));

	auto task_pre = task_cl::task_cl_kernel_pre::make_task(args);
	auto task_exec = task_cl::task_cl_kernel_exec::make_task(args, "cltest.cl", "CLTest2", 1024*1024/16, 256);
	auto task_post = task_cl::task_cl_kernel_post::make_task(args);

	parallel_f::task_queue tq;

	tq.push(task_pre);
	tq.push(task_exec);
	tq.push(task_post);

	tq.exec();

	free(data);
}


static void test_cl2()
{
	std::vector<std::byte> src(1024*1024), dst(1024*1024);

	task_cl::task_cl_kernel_args kernel_args(new task_cl::task_cl_kernel_args::kernel_arg_mem(src.size(), src.data(), NULL),
											 new task_cl::task_cl_kernel_args::kernel_arg_mem(dst.size(), NULL, dst.data()),
											 new task_cl::task_cl_kernel_args::kernel_arg_t<cl_int>((cl_int)src.size() / 16));

	auto task = task_cl::make_task("cltest.cl", "CLTest2", src.size() / 16, 256, kernel_args);

	task->finish();
}


static void test_cl_objects()
{
	class object
	{
	public:
		int x;
		int y;
		int seq;
	};

	std::vector<object> objects(100);

	parallel_f::task_list tl;
	parallel_f::task_id flush_id = 0;

	for (int i = 0; i < 20; i++) {
		task_cl::task_cl_kernel_args kernel_args(new task_cl::task_cl_kernel_args::kernel_arg_mem(objects.size() * sizeof(object),
			objects.data(), objects.data()),
			new task_cl::task_cl_kernel_args::kernel_arg_t<cl_int>((cl_int)objects.size()));

		auto task = task_cl::make_task("cltest.cl", "RunObjects", objects.size(), objects.size(), kernel_args);

		auto run_id = tl.append(task, flush_id);

		auto log_task = parallel_f::make_task([&objects]() {
			parallel_f::logInfo("object seq %d\n", objects[0].seq);

			return parallel_f::none;
		});

		tl.append(log_task, run_id);

		flush_id = tl.flush();
	}

	tl.finish();
}


static void test_objects()
{
	class object
	{
	public:
		int x;
		int y;
		int seq;
	};

	std::vector<object> objects(1000000);

	parallel_f::task_list tl;
	parallel_f::task_id flush_id = 0;

	for (int i = 0; i < 10; i++) {
		auto task = parallel_f::make_task([&objects]() {
			for (int i = 0; i < objects.size(); i++)
				objects[i].seq++;

			return parallel_f::none;
		});

		auto run_id = tl.append(task, flush_id);

		auto log_task = parallel_f::make_task([&objects]() {
			parallel_f::logInfo("object x %d, y %d, seq %d\n", objects[0].x, objects[1].y, objects[1].seq);

			return parallel_f::none;
			});

		tl.append(log_task, run_id);

		flush_id = tl.flush();
	}

	tl.finish();
}
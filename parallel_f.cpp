// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <fstream>

#include "parallel_f.hpp"
#include "task_cl.hpp"



static void test_extract_todos(std::string initial_file);
static void test_nested_tasking();
static void test_cl_kernel();
static void test_cl2();
static void test_cl_objects();


int main()
{
	parallel_f::setDebugLevel(0);

	test_extract_todos("parallel_f.cpp");
	parallel_f::stats::instance::get().show_stats();

//	test_cl_kernel();
//	parallel_f::stats::instance::get().show_stats();

//	test_cl2();
//	parallel_f::stats::instance::get().show_stats();
}


// parallel_f :: task_list/queue == testing example that extracts all todos from sources

static void test_extract_todos(std::string initial_file)
{
	auto readfile_func = [](auto filename)
	{
		parallel_f::logInfo( "  <= ::: => reading source code (file path) %s\n", filename.c_str() );

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

			parallel_f::logInfo( "     ->> local include at position (file offset) %d <- %s\n", pos, inc_filename.c_str() );

			included_files.push_back(inc_filename);
		}

		return included_files;
	};


	std::map<std::string,bool> files;

	auto loop_func = [&files](auto t1)
	{
		std::list<std::string> included_files = t1.get<std::list<std::string>>();

		for (auto& li : included_files) {
			auto it = files.find(li);

			if (it == files.end())
				files.insert(std::make_pair(li, false));
		}

		return std::any();
	};


	files.insert(std::make_pair(initial_file, false));

	while (true) {
		bool done = true;
		parallel_f::task_list tl;
		parallel_f::task_id loop_id = 0;

		for (auto it : files) {
			if (!it.second) {
				files[it.first] = true;

				done = false;


				parallel_f::task_id id;


				auto readtask = parallel_f::make_task(readfile_func, it.first);

				id = tl.append(readtask);


				auto parsetask = parallel_f::make_task(parsestring_func, readtask->result());

				id = tl.append(parsetask, id);


				auto looptask = parallel_f::make_task(loop_func, parsetask->result());

				loop_id = tl.append(looptask, id, loop_id);
			}
		}
	
		tl.finish();

		if (done)
			break;
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

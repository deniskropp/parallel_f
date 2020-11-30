// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <fstream>

#include "parallel_f.hpp"



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

//	test_queue();

//	test_list();

//	test_extract_todos("parallel_f.cpp");

//	test_nested_tasking();

//	test_cl_kernel();

//	test_cl2();

	test_objects();
}


// parallel_f :: task_queue == testing example

static void test_queue()
{
	auto func1 = []() -> std::string
	{
		parallel_f::logInfo("First function being called (initial task)\n");

		return "Hello World";
	};

	auto func2 = [](auto msg) -> std::string
	{
		parallel_f::logInfo("Second function receiving '%s' from previous task\n", msg.get<std::string>().c_str());

		return "Good bye";
	};

	auto func3 = [](auto msg)
	{
		parallel_f::logInfo("Third task running with '%s' from second...\n", msg.get<std::string>().c_str());

		return 0;
	};

	// round 1 = initial test with three tasks
	auto task1 = parallel_f::make_task(func1);
	auto task2 = parallel_f::make_task(func2, task1->result());
	auto task3 = parallel_f::make_task(func3, task2->result());

	parallel_f::task_queue tq;

	tq.push(task1);
	tq.push(task2);
	tq.push(task3);

	tq.exec();


	// round 2 = push (reset) all of our tasks again, different order
	tq.push(task3);
	tq.push(task2);
	tq.push(task1);

	tq.exec();


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

	parallel_f::joinable j = tq.exec(true);
	
	j.join();
}


// parallel_f :: task_list == testing example

static void test_list()
{
	auto func = [](auto a)
	{
		parallel_f::logInfo( "Function %s\n", a );

		return 0;
	};

	auto task1 = parallel_f::make_task(func, "running task1...");
	auto task2 = parallel_f::make_task(func, "running task2...");
	auto task3 = parallel_f::make_task(func, "running task3...");
	auto task4 = parallel_f::make_task(func, "running task4...");
	auto task5 = parallel_f::make_task(func, "running task5...");


	parallel_f::task_list tl;

	auto a1_id = tl.append(task1);
	auto a2_id = tl.append(task2, a1_id);
	auto a3_id = tl.append(task3, a1_id);
	auto a4_id = tl.append(task4, a2_id, a3_id);
	auto a5_id = tl.append(task5, a4_id);

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


// parallel_f :: task_queues == testing nested execution

static void test_nested_tasking()
{
	auto main_func = []() {
		auto sub = parallel_f::make_task([]() {
				parallel_f::logInfo("sub task calling vthread::yield()...\n");
				parallel_f::vthread::yield();
				parallel_f::logInfo("sub task vthread::yield() done.\n");

				return 0;
			});

		parallel_f::task_queue tq;

		tq.push(sub);

		parallel_f::logInfo("main task running exec() and join()...\n");

		tq.exec(true).join();

		return 0;
	};

	auto main_task = parallel_f::make_task(main_func);

	parallel_f::task_queue tq;

	tq.push(main_task);

	tq.exec(true).join();
}



#define CL_TARGET_OPENCL_VERSION 120
#include <CL\opencl.h>

#include <OCL_Device.h>

namespace task_cl {

class system
{
public:
	static system& instance()
	{
		static system system_instance;

		return system_instance;
	}

private:
	OCL_Device* pOCL_Device;
	std::mutex lock;

public:
	system()
	{
		// Parse arguments
		// OpenCL arguments: platform and device
		int iPlatform = 0;// GetArgInt(argc, argv, "p");
		int iDevice = 0;// GetArgInt(argc, argv, "d");

		parallel_f::logInfo("Initializing OpenCL...\n");

		// Set-up OpenCL Platform
		pOCL_Device = new OCL_Device(iPlatform, iDevice);
		pOCL_Device->SetBuildOptions("");
		pOCL_Device->PrintInfo();
	}

	~system()
	{
		delete pOCL_Device;
	}

	OCL_Device* getDevice()
	{
		return pOCL_Device;
	}

	std::mutex& getLock()
	{
		return lock;
	}
};


class task_cl_kernel_args
{
public:
	class kernel_arg
	{
	public:
		virtual void kernel_pre_init(OCL_Device* pOCL_Device, int idx) {}
		virtual void kernel_pre_run(OCL_Device* pOCL_Device, int idx) {}
		virtual void kernel_exec_init(cl_kernel Kernel, int idx) {}
		virtual void kernel_post_run(OCL_Device* pOCL_Device, int idx) {}
		virtual void kernel_post_deinit(OCL_Device* pOCL_Device, int idx) {}
	};

	template <typename T>
	class kernel_arg_t : public kernel_arg
	{
	public:
		T arg;
		
		kernel_arg_t<T>(T arg) : arg(arg) {}

		virtual void kernel_exec_init(cl_kernel Kernel, int idx)
		{
			cl_int err;
			err = clSetKernelArg(Kernel, idx, sizeof(cl_int), &arg);
			CHECK_OPENCL_ERROR(err);
		}
	};

	class kernel_arg_mem : public kernel_arg
	{
	public:
		size_t size;
		void* host_in;
		void* host_out;

		cl_mem mem;

		kernel_arg_mem(size_t size, void* host_in, void* host_out) : size(size), host_in(host_in), host_out(host_out)
		{
			memset(&mem, 0, sizeof(mem));
		}

		virtual void kernel_pre_init(OCL_Device* pOCL_Device, int idx)
		{
			// Allocate Device Memory
			mem = pOCL_Device->DeviceMalloc(idx, size);
		}

		virtual void kernel_pre_run(OCL_Device* pOCL_Device, int idx)
		{
			if (host_in)
				pOCL_Device->CopyBufferToDevice(host_in, idx, size);
		}

		virtual void kernel_exec_init(cl_kernel Kernel, int idx)
		{
			cl_int err;
			err = clSetKernelArg(Kernel, idx, sizeof(cl_mem), &mem);
			CHECK_OPENCL_ERROR(err);
		}

		virtual void kernel_post_run(OCL_Device* pOCL_Device, int idx)
		{
			if (host_out)
				pOCL_Device->CopyBufferToHost(host_out, idx, size);
		}

		virtual void kernel_post_deinit(OCL_Device* pOCL_Device, int idx)
		{
			pOCL_Device->DeviceFree(idx);
		}
	};

	template <typename T>
	class kernel_arg_value_t : public kernel_arg
	{
	public:
		parallel_f::task_info::Value value;

		kernel_arg_value_t<T>(parallel_f::task_info::Value value) : value(value) {}
	};

	class kernel_arg_value_mem : public kernel_arg
	{
	public:
		parallel_f::task_info::Value value;

		kernel_arg_value_mem(parallel_f::task_info::Value value) : value(value) {}
	};

public:
	std::vector<std::shared_ptr<kernel_arg>> args;

	template <typename... kargs>
	task_cl_kernel_args(kargs*... args)
	{
		(this->args.push_back(std::shared_ptr<kernel_arg>(args)), ...);
	}
};


class task_cl_kernel_pre : public parallel_f::task_base
{
public:
	static auto make_task(task_cl_kernel_args& args)
	{
		return std::make_shared<task_cl_kernel_pre>(args);
	}

public:
	task_cl_kernel_args& args;

	task_cl_kernel_pre(task_cl_kernel_args& args) : args(args)
	{
		std::unique_lock<std::mutex> lock(system::instance().getLock());

		OCL_Device* pOCL_Device = system::instance().getDevice();

		for (int i=0; i<args.args.size(); i++)
			args.args[i]->kernel_pre_init(pOCL_Device, i);

		// Allocate Device Memory
//		cl_mem d_A = pOCL_Device->DeviceMalloc(0, test_bytes);
//		cl_mem d_B = pOCL_Device->DeviceMalloc(1, test_bytes);
	}

protected:
	virtual void run()
	{
		std::unique_lock<std::mutex> lock(system::instance().getLock());

		OCL_Device* pOCL_Device = system::instance().getDevice();

		for (int i = 0; i < args.args.size(); i++)
			args.args[i]->kernel_pre_run(pOCL_Device, i);
		
		// host to device copy etc.
//		pOCL_Device->CopyBufferToDevice(test_data, 0, test_bytes);
	}
};


class task_cl_kernel_exec : public parallel_f::task_base
{
public:
	static auto make_task(task_cl_kernel_args& args,
						  std::string programName, std::string kernelName,
						  size_t global_work_size, size_t local_work_size)
	{
		return std::make_shared<task_cl_kernel_exec>(args, programName, kernelName, global_work_size, local_work_size);
	}

public:
	task_cl_kernel_args& args;
	cl_kernel Kernel;
	size_t global_work_size;
	size_t local_work_size;

	task_cl_kernel_exec(task_cl_kernel_args& args, std::string programName, std::string kernelName,
						size_t global_work_size, size_t local_work_size)
		:
		args(args),
		global_work_size(global_work_size),
		local_work_size(local_work_size)
	{
		std::unique_lock<std::mutex> lock(system::instance().getLock());

		printf("Building Kernel...\n");

		//cl_int err;
		OCL_Device* pOCL_Device = system::instance().getDevice();

		// Set up OpenCL 
		Kernel = pOCL_Device->GetKernel(programName.c_str(), kernelName.c_str());

		// Set Kernel Arguments
		for (int i = 0; i < args.args.size(); i++)
			args.args[i]->kernel_exec_init(Kernel, i);

//		cl_int _num = test_bytes / 16;
//		err = clSetKernelArg(Kernel, 0, sizeof(cl_mem), &d_A);
//		CHECK_OPENCL_ERROR(err);
//		err = clSetKernelArg(Kernel, 1, sizeof(cl_mem), &d_B);
//		CHECK_OPENCL_ERROR(err);
//		err = clSetKernelArg(Kernel, 2, sizeof(cl_int), &_num);
//		CHECK_OPENCL_ERROR(err);
	}

protected:
	virtual void run()
	{
		std::unique_lock<std::mutex> lock(system::instance().getLock());

		parallel_f::logInfo("task_cl_kernel_exec::run()\n");
		
		cl_int err;
		OCL_Device* pOCL_Device = system::instance().getDevice();

		// Wait for previous action to finish
		err = clFinish(pOCL_Device->GetQueue());
		CHECK_OPENCL_ERROR(err);

		printf("Running Kernel...\n");

		// Run the kernel
		err = clEnqueueNDRangeKernel(pOCL_Device->GetQueue(), Kernel, 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL);
		CHECK_OPENCL_ERROR(err);

		// Wait for kernel to finish
		err = clFinish(pOCL_Device->GetQueue());
		CHECK_OPENCL_ERROR(err);
	}
};


class task_cl_kernel_post : public parallel_f::task_base
{
public:
	static auto make_task(task_cl_kernel_args& args)
	{
		return std::make_shared<task_cl_kernel_post>(args);
	}

public:
	task_cl_kernel_args& args;

	task_cl_kernel_post(task_cl_kernel_args& args) : args(args)
	{
	}

	virtual ~task_cl_kernel_post()
	{
		std::unique_lock<std::mutex> lock(system::instance().getLock());

		OCL_Device* pOCL_Device = system::instance().getDevice();

		for (int i = 0; i < args.args.size(); i++)
			args.args[i]->kernel_post_deinit(pOCL_Device, i);
	}

protected:
	virtual void run()
	{
		std::unique_lock<std::mutex> lock(system::instance().getLock());

		OCL_Device* pOCL_Device = system::instance().getDevice();

		for (int i = 0; i < args.args.size(); i++)
			args.args[i]->kernel_post_run(pOCL_Device, i);

//		pOCL_Device->CopyBufferToHost(test_data, 1, test_bytes);
	}
};

}


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


namespace task_cl {

static auto make_task(std::string file, std::string kernel, size_t global_work_size,
					  size_t local_work_size, task_cl::task_cl_kernel_args& kernel_args)
{
	auto task_pre = task_cl::task_cl_kernel_pre::make_task(kernel_args);
	auto task_exec = task_cl::task_cl_kernel_exec::make_task(kernel_args, file, kernel, global_work_size, local_work_size);
	auto task_post = task_cl::task_cl_kernel_post::make_task(kernel_args);
	
	parallel_f::logInfo("task_cl::make_task()\n");

	auto func = [task_pre,task_exec,task_post,kernel]() {
		parallel_f::task_queue tq;
		
		parallel_f::logInfo("task_cl('%s')\n", kernel.c_str());
		
		tq.push(task_pre);
		tq.push(task_exec);
		tq.push(task_post);

		tq.exec();

		return parallel_f::none;
	};

	return parallel_f::make_task(func);
}

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

	std::vector<object> objects(100);

	parallel_f::task_list tl;
	parallel_f::task_id flush_id = 0;

	for (int i = 0; i < 20; i++) {
		auto task = parallel_f::make_task([&objects]() {
			for (int i = 0; i < 100; i++)
				objects[i].seq++;

			return parallel_f::none;
		});

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
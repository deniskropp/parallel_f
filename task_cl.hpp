// === (C) 2020 === parallel_f / task_cl (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include "parallel_f.hpp"

#include "OCL_Device.h"
#include "Util.h"


namespace task_cl {

class system
{
public:
	static system& instance()
	{
		static system system_instance;

		return system_instance;
	}

public:
	class QueueElement
	{
	public:
		cl_command_queue queue;
		std::function<void(void)> done;

		QueueElement(cl_command_queue queue, std::function<void(void)> done)
			:
			queue(queue),
			done(done)
		{
		}
	};

private:
	OCL_Device* pOCL_Device;
	std::mutex lock;
	std::condition_variable cond;
	std::thread* thread;
	bool stop;
	std::queue<QueueElement> queue;

public:
	void pushQueue(cl_command_queue command_queue, std::function<void(void)> done)
	{
		std::unique_lock<std::mutex> l(lock);

		queue.push(QueueElement(command_queue, done));

		cond.notify_all();
	}

public:
	system()
		:
		stop(false)
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

		thread = new std::thread([this]() {
			std::unique_lock<std::mutex> l(lock);
			
			while (!stop) {
				if (!queue.empty()) {
					auto e = queue.front();

					queue.pop();

					l.unlock();

					clFinish(e.queue);

					e.done();

					l.lock();
				}
				else
					cond.wait(l);
			}
		});
	}

	~system()
	{
		std::unique_lock<std::mutex> l(lock);

		stop = true;

		cond.notify_all();

		l.unlock();

		thread->join();

		delete thread;

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


class kernel
{
	friend class task_cl_kernel_exec;

private:
	cl_kernel clkernel;
	size_t global_work_size;
	size_t local_work_size;

public:
	kernel(cl_kernel kernel, size_t global_work_size, size_t local_work_size)
		:
		clkernel(kernel),
		global_work_size(global_work_size),
		local_work_size(local_work_size)
	{
	}
};


kernel make_kernel(std::string file, std::string name, size_t global_work_size, size_t local_work_size)
{
	std::unique_lock<std::mutex> lock(system::instance().getLock());

	OCL_Device* pOCL_Device = system::instance().getDevice();

	cl_kernel k = pOCL_Device->GetKernel(file.c_str(), name.c_str());

	return kernel(k, global_work_size, local_work_size);
}


class task_cl_kernel_args
{
public:
	OCL_Device* device;

	OCL_Device* get_device()
	{
		if (!device)
			device = new OCL_Device(system::instance().getDevice());

		return device;
	}

	class kernel_arg
	{
	public:
		virtual void kernel_pre_init(OCL_Device* pOCL_Device, int idx) {}
		virtual void kernel_pre_run(OCL_Device* pOCL_Device, int idx) {}
		virtual void kernel_exec_run(cl_kernel Kernel, int idx) {}
		virtual void kernel_post_run(OCL_Device* pOCL_Device, int idx) {}
		virtual void kernel_post_deinit(OCL_Device* pOCL_Device, int idx) {}
	};

	template <typename T>
	class kernel_arg_t : public kernel_arg
	{
	public:
		T arg;
		
		kernel_arg_t<T>(T arg) : arg(arg) {}

		virtual void kernel_exec_run(cl_kernel Kernel, int idx)
		{
			cl_int err;
			err = clSetKernelArg(Kernel, idx, sizeof(T), &arg);
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
			parallel_f::logInfo("task_cl: Malloc(%d)\n", idx);

			mem = pOCL_Device->DeviceMalloc(idx, size);
		}

		virtual void kernel_pre_run(OCL_Device* pOCL_Device, int idx)
		{
			if (host_in)
				pOCL_Device->CopyBufferToDevice(host_in, idx, size);
		}

		virtual void kernel_exec_run(cl_kernel Kernel, int idx)
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
			parallel_f::logInfo("task_cl: Free(%d)\n", idx);
			
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
		:
		device(0)
	{
		(this->args.push_back(std::shared_ptr<kernel_arg>(args)), ...);
	}

	~task_cl_kernel_args()
	{
		if (device)
			delete device;
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
		//std::unique_lock<std::mutex> lock(system::instance().getLock());

		OCL_Device* pOCL_Device = args.get_device();//system::instance().getDevice();

		for (int i=0; i<args.args.size(); i++)
			args.args[i]->kernel_pre_init(pOCL_Device, i);

		// Allocate Device Memory
//		cl_mem d_A = pOCL_Device->DeviceMalloc(0, test_bytes);
//		cl_mem d_B = pOCL_Device->DeviceMalloc(1, test_bytes);
	}

protected:
	virtual bool run()
	{
		//std::unique_lock<std::mutex> lock(system::instance().getLock());

		parallel_f::logInfo("task_cl_kernel_pre::run()...\n");

		OCL_Device* pOCL_Device = args.get_device();//system::instance().getDevice();

		for (int i = 0; i < args.args.size(); i++)
			args.args[i]->kernel_pre_run(pOCL_Device, i);
		
		// host to device copy etc.
//		pOCL_Device->CopyBufferToDevice(test_data, 0, test_bytes);

//		cl_int err = clFinish(pOCL_Device->GetQueue());
//		CHECK_OPENCL_ERROR(err);

		system::instance().pushQueue(pOCL_Device->GetQueue(), [this]() {
			parallel_f::logInfo("task_cl_kernel_pre::run() cl done.\n");

			enter_state(task_state::FINISHED);
		});

		parallel_f::logInfo("task_cl_kernel_pre::run() done.\n");

		return false;
	}
};


class task_cl_kernel_exec : public parallel_f::task_base
{
public:
	static auto make_task(task_cl_kernel_args& args, kernel kernel)
	{
		return std::make_shared<task_cl_kernel_exec>(args, kernel);
	}

public:
	task_cl_kernel_args& args;
	task_cl::kernel kernel;

	task_cl_kernel_exec(task_cl_kernel_args& args, task_cl::kernel kernel)
		:
		args(args),
		kernel(kernel)
	{
	}

protected:
	virtual bool run()
	{
		//std::unique_lock<std::mutex> lock(system::instance().getLock());

		parallel_f::logInfo("task_cl_kernel_exec::run()...\n");
		
		cl_int err;
		OCL_Device* pOCL_Device = args.get_device();//system::instance().getDevice();

		// Set Kernel Arguments
		for (int i = 0; i < args.args.size(); i++)
			args.args[i]->kernel_exec_run(kernel.clkernel, i);

		// Run the kernel
		err = clEnqueueNDRangeKernel(pOCL_Device->GetQueue(), kernel.clkernel,
									 1, NULL, &kernel.global_work_size, &kernel.local_work_size, 0, NULL, NULL);
		CHECK_OPENCL_ERROR(err);

		// Wait for kernel to finish
//		err = clFinish(pOCL_Device->GetQueue());
//		CHECK_OPENCL_ERROR(err);

		system::instance().pushQueue(pOCL_Device->GetQueue(), [this]() {
			parallel_f::logInfo("task_cl_kernel_exec::run() cl done.\n");

			enter_state(task_state::FINISHED);
		});

		parallel_f::logInfo("task_cl_kernel_exec::run() done.\n");

		return false;
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
		//std::unique_lock<std::mutex> lock(system::instance().getLock());

		OCL_Device* pOCL_Device = args.get_device();//system::instance().getDevice();

		for (int i = 0; i < args.args.size(); i++)
			args.args[i]->kernel_post_deinit(pOCL_Device, i);
	}

protected:
	virtual bool run()
	{
		//std::unique_lock<std::mutex> lock(system::instance().getLock());

		parallel_f::logInfo("task_cl_kernel_post::run()...\n");

		OCL_Device* pOCL_Device = args.get_device();//system::instance().getDevice();

		for (int i = 0; i < args.args.size(); i++)
			args.args[i]->kernel_post_run(pOCL_Device, i);

//		pOCL_Device->CopyBufferToHost(test_data, 1, test_bytes);

//		cl_int err = clFinish(pOCL_Device->GetQueue());
//		CHECK_OPENCL_ERROR(err);

		system::instance().pushQueue(pOCL_Device->GetQueue(), [this]() {
			parallel_f::logInfo("task_cl_kernel_post::run() cl done.\n");

			enter_state(task_state::FINISHED);
		});

		parallel_f::logInfo("task_cl_kernel_post::run() done.\n");

		return false;
	}
};


static auto make_task(std::string file, std::string name, size_t global_work_size,
					  size_t local_work_size, const task_cl_kernel_args& kernel_args)
{
	parallel_f::logDebug("task_cl::make_task()\n");

	auto kernel = make_kernel(file, name, global_work_size, local_work_size);

	auto args = std::make_shared<task_cl_kernel_args>(kernel_args);

	auto task_pre = task_cl_kernel_pre::make_task(*args);
	auto task_exec = task_cl_kernel_exec::make_task(*args, kernel);
	auto task_post = task_cl_kernel_post::make_task(*args);
	
	auto func = [](auto task_pre, auto task_exec, auto task_post, auto kernel, auto args) {
		parallel_f::task_list tl;
		
		auto t1_id = tl.append(task_pre);
		auto t2_id = tl.append(task_exec, t1_id);
		auto t3_id = tl.append(task_post, t2_id);

		tl.finish();
	};

	return parallel_f::make_task(func, task_pre, task_exec, task_post, kernel, args);
}

}

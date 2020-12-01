// === (C) 2020 === parallel_f / task_cl (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include "parallel_f.hpp"


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


static auto make_task(std::string file, std::string kernel, size_t global_work_size,
					  size_t local_work_size, task_cl::task_cl_kernel_args& kernel_args)
{
	auto task_pre = task_cl::task_cl_kernel_pre::make_task(kernel_args);
	auto task_exec = task_cl::task_cl_kernel_exec::make_task(kernel_args, file, kernel, global_work_size, local_work_size);
	auto task_post = task_cl::task_cl_kernel_post::make_task(kernel_args);
	
	parallel_f::logDebug("task_cl::make_task()\n");

	auto func = [task_pre,task_exec,task_post,kernel]() {
		parallel_f::task_queue tq;
		
		parallel_f::logDebug("task_cl('%s')\n", kernel.c_str());
		
		tq.push(task_pre);
		tq.push(task_exec);
		tq.push(task_post);

		tq.exec();

		return parallel_f::none;
	};

	return parallel_f::make_task(func);
}

}

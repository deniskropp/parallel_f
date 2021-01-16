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
	std::vector<std::thread*> threads;
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

		spawnQueueThread();
	}

	void spawnQueueThread()
	{
		if (threads.size() >= std::thread::hardware_concurrency())
			return;

		auto stat = parallel_f::stats::instance::get().make_stat(std::string("cl.") + std::to_string(threads.size()));

		threads.push_back(new std::thread([this,stat]() {
			parallel_f::sysclock clock;
			std::unique_lock<std::mutex> l(lock);
			
			while (!stop) {
				if (queue.empty()) {
					cond.wait(l);

					l.unlock();

					stat->report_idle(clock.reset());
				}
				else {
					auto e = queue.front();

					queue.pop();

					if (!queue.empty())
						spawnQueueThread();

					l.unlock();

					clFinish(e.queue);

					e.done();
				}

				l.lock();

				stat->report_busy(clock.reset());
			}
		}));
	}

	~system()
	{
		std::unique_lock<std::mutex> l(lock);

		stop = true;

		cond.notify_all();

		l.unlock();

		for (auto t : threads) {
			t->join();

			delete t;
		}

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
	friend class kernel_exec;

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


std::shared_ptr<kernel> make_kernel(std::string file, std::string name, size_t global_work_size, size_t local_work_size)
{
	std::unique_lock<std::mutex> lock(system::instance().getLock());

	OCL_Device* pOCL_Device = system::instance().getDevice();

	cl_kernel k = pOCL_Device->GetKernel(file, name);

	return std::make_shared<kernel>(k, global_work_size, local_work_size);
}


class kernel_args
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
		virtual void kernel_pre_run(OCL_Device* pOCL_Device, cl_command_queue queue, int idx) {}
		virtual void kernel_exec_run(cl_kernel Kernel, int idx) {}
		virtual void kernel_post_run(OCL_Device* pOCL_Device, cl_command_queue queue, int idx) {}
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

		std::shared_ptr<OCL_Buffer> buffer;

		kernel_arg_mem(size_t size, void* host_in, void* host_out) : size(size), host_in(host_in), host_out(host_out)
		{
		}

		virtual void kernel_pre_init(OCL_Device* pOCL_Device, int idx)
		{
			parallel_f::logDebug("task_cl: Malloc(%d)\n", idx);

			buffer = pOCL_Device->CreateBuffer(size ? size : 1);
		}

		virtual void kernel_pre_run(OCL_Device* pOCL_Device, cl_command_queue queue, int idx)
		{
			if (host_in)
				buffer->CopyBufferToDevice(queue, host_in, size);
		}

		virtual void kernel_exec_run(cl_kernel Kernel, int idx)
		{
			cl_mem mem = buffer->get();

			cl_int err;
			err = clSetKernelArg(Kernel, idx, sizeof(cl_mem), &mem);
			CHECK_OPENCL_ERROR(err);
		}

		virtual void kernel_post_run(OCL_Device* pOCL_Device, cl_command_queue queue, int idx)
		{
			if (host_out)
				buffer->CopyBufferToHost(queue, host_out, size);
		}

		virtual void kernel_post_deinit(OCL_Device* pOCL_Device, int idx)
		{
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
	kernel_args(kargs*... args)
		:
		device(0)
	{
		(this->args.push_back(std::shared_ptr<kernel_arg>(args)), ...);
	}

	~kernel_args()
	{
		if (device)
			delete device;
	}

	std::shared_ptr<kernel_arg> operator [](size_t index)
	{
		return args[index];
	}

	template <typename T>
	void set(size_t index, T value)
	{
		kernel_arg* arg = args[index].get();

		kernel_arg_t<T>* arg_t = static_cast<kernel_arg_t<T>*>(arg);
		
		arg_t->arg = value;
	}
};


class kernel_pre : public parallel_f::task_base, public std::enable_shared_from_this<kernel_pre>
{
public:
	static auto make_task(std::shared_ptr<kernel_args> args)
	{
		return std::make_shared<kernel_pre>(args);
	}

public:
	std::shared_ptr<kernel_args> args;
	cl_command_queue queue;

	kernel_pre(std::shared_ptr<kernel_args> args) : args(args)
	{
		parallel_f::logDebug("task_cl::kernel_pre::kernel_pre()...\n");

		OCL_Device* pOCL_Device = args->get_device();

		queue = pOCL_Device->CreateQueue();

		for (int i=0; i<args->args.size(); i++)
			args->args[i]->kernel_pre_init(pOCL_Device, i);
	
		parallel_f::logDebug("task_cl::kernel_pre::kernel_pre() done.\n");
	}

	virtual ~kernel_pre()
	{
		parallel_f::logDebug("task_cl::kernel_pre::~kernel_pre()\n");

		OCL_Device* pOCL_Device = args->get_device();

		pOCL_Device->DestroyQueue(queue);
	}

protected:
	virtual bool run()
	{
		parallel_f::logDebug("task_cl::kernel_pre::run()...\n");

		OCL_Device* pOCL_Device = args->get_device();

		for (int i = 0; i < args->args.size(); i++)
			args->args[i]->kernel_pre_run(pOCL_Device, queue, i);
		

		auto shared_this = shared_from_this();

		system::instance().pushQueue(queue, [shared_this]() {
			parallel_f::logDebug("task_cl::kernel_pre::run() <= Queue FINISHED\n");

			shared_this->enter_state(task_state::FINISHED);
		});

		parallel_f::logDebug("task_cl::kernel_pre::run() done.\n");

		return false;
	}
};


class kernel_exec : public parallel_f::task_base, public std::enable_shared_from_this<kernel_exec>
{
public:
	static auto make_task(std::shared_ptr<kernel_args> args, std::shared_ptr<kernel> kernel)
	{
		return std::make_shared<kernel_exec>(args, kernel);
	}

public:
	std::shared_ptr<kernel_args> args;
	cl_command_queue queue;
	std::shared_ptr<task_cl::kernel> kernel;

	kernel_exec(std::shared_ptr<kernel_args> args, std::shared_ptr<task_cl::kernel> kernel)
		:
		args(args),
		kernel(kernel)
	{
		parallel_f::logDebug("task_cl::kernel_exec::kernel_exec()...\n");

		OCL_Device* pOCL_Device = args->get_device();

		queue = pOCL_Device->CreateQueue();

		parallel_f::logDebug("task_cl::kernel_exec::kernel_exec() done.\n");
	}

	virtual ~kernel_exec()
	{
		parallel_f::logDebug("task_cl::kernel_exec::~kernel_exec()\n");

		OCL_Device* pOCL_Device = args->get_device();

		pOCL_Device->DestroyQueue(queue);
	}

protected:
	virtual bool run()
	{
		parallel_f::logDebug("task_cl::kernel_exec::run()...\n");
		
		cl_int err;
		OCL_Device* pOCL_Device = args->get_device();

		// Set Kernel Arguments
		for (int i = 0; i < args->args.size(); i++)
			args->args[i]->kernel_exec_run(kernel->clkernel, i);

		// Run the kernel
		err = clEnqueueNDRangeKernel(queue, kernel->clkernel,
									 1, NULL, &kernel->global_work_size, &kernel->local_work_size, 0, NULL, NULL);
		CHECK_OPENCL_ERROR(err);

		
		auto shared_this = shared_from_this();

		system::instance().pushQueue(queue, [shared_this]() {
			parallel_f::logDebug("task_cl::kernel_exec::run() <= Queue FINISHED\n");

			shared_this->enter_state(task_state::FINISHED);
		});

		parallel_f::logDebug("task_cl::kernel_exec::run() done.\n");

		return false;
	}
};


class kernel_post : public parallel_f::task_base, public std::enable_shared_from_this<kernel_post>
{
public:
	static auto make_task(std::shared_ptr<kernel_args> args)
	{
		return std::make_shared<kernel_post>(args);
	}

public:
	std::shared_ptr<kernel_args> args;
	cl_command_queue queue;

	kernel_post(std::shared_ptr<kernel_args> args) : args(args)
	{
		parallel_f::logDebug("task_cl::kernel_post::kernel_post()...\n");

		OCL_Device* pOCL_Device = args->get_device();

		queue = pOCL_Device->CreateQueue();
	
		parallel_f::logDebug("task_cl::kernel_post::kernel_post() done.\n");
	}

	virtual ~kernel_post()
	{
		parallel_f::logDebug("task_cl::kernel_post::~kernel_post()\n");

		OCL_Device* pOCL_Device = args->get_device();

		for (int i = 0; i < args->args.size(); i++)
			args->args[i]->kernel_post_deinit(pOCL_Device, i);

		pOCL_Device->DestroyQueue(queue);
	}

protected:
	virtual bool run()
	{
		parallel_f::logDebug("task_cl::kernel_post::run()...\n");

		OCL_Device* pOCL_Device = args->get_device();

		for (int i = 0; i < args->args.size(); i++)
			args->args[i]->kernel_post_run(pOCL_Device, queue, i);


		auto shared_this = shared_from_this();

		system::instance().pushQueue(queue, [shared_this]() {
			parallel_f::logDebug("task_cl::kernel_post::run() <= Queue FINISHED\n");

			shared_this->enter_state(task_state::FINISHED);
		});

		parallel_f::logDebug("task_cl::kernel_post::run() done.\n");

		return false;
	}
};


class cl_task : public parallel_f::task_base, public std::enable_shared_from_this<cl_task>
{
private:
	std::shared_ptr<kernel> kernel;
	std::shared_ptr<kernel_args> args;
	std::shared_ptr<task_base> task_pre;
	std::shared_ptr<task_base> task_exec;
	std::shared_ptr<task_base> task_post;

public:
	cl_task(std::shared_ptr<task_cl::kernel> kernel, std::shared_ptr<kernel_args> args)
		:
		kernel(kernel),
		args(args)
	{
		task_pre = kernel_pre::make_task(args);
		task_exec = kernel_exec::make_task(args, kernel);
		task_post = kernel_post::make_task(args);
	}

protected:
	virtual bool run()
	{
		parallel_f::logDebug("task_cl::cl_task running...\n");

		parallel_f::task_queue tq;

		tq.push(task_pre);
		tq.push(task_exec);
		tq.push(task_post);

		auto shared_this = shared_from_this();

		tq.push(parallel_f::make_task([shared_this]() {
			parallel_f::logDebug("task_cl::cl_task FINISHED\n");

			shared_this->enter_state(task_state::FINISHED);
		}));

		tq.exec(true);

		parallel_f::logDebug("task_cl::cl_task run done.\n");

		return false;
	}
};

static auto make_task(std::shared_ptr<kernel> kernel, std::shared_ptr<kernel_args> args)
{
	parallel_f::logDebug("task_cl::make_task()\n");

	return std::make_shared<cl_task>(kernel, args);
}

}

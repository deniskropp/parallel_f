#ifndef _OCL_DEVICE_H_
#define _OCL_DEVICE_H_

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/opencl.h>


class OCL_Buffer
{
private:
	cl_mem mem;

public:
	OCL_Buffer(cl_mem mem);
	~OCL_Buffer();

	cl_mem get() const { return mem; }

	void CopyBufferToDevice(cl_command_queue queue, void* h_Buffer, size_t size);
	void CopyBufferToHost(cl_command_queue queue, void* h_Buffer, size_t size);
};

class OCL_Device
{
private:
	cl_platform_id		m_platform_id;
	cl_device_id		m_device_id;
	cl_context			m_context;

	std::string m_sBuildOptions;

	std::mutex m_kernels_lock;
	std::mutex m_queues_lock;

	// Uniquely maps a kernel via a program name and a kernel name
	std::map<std::string, std::pair<cl_program, 
		std::map<std::string, cl_kernel> > > m_kernels;
	
	std::list<cl_command_queue> m_queues_list;

	cl_program GetProgramFromFile(std::string filename);
	cl_program GetProgram(std::string source);

public:
	OCL_Device(int iPlatformNum, int iDeviceNum);
	~OCL_Device(void);

	cl_context GetContext();

	void PrintInfo(void);

	cl_command_queue CreateQueue();
	void DestroyQueue(cl_command_queue queue);

	void SetBuildOptions (const char* sBuildOptions);
	cl_kernel GetKernel(std::string filename, std::string sKernelName);
	cl_kernel GetKernelFromSource(std::string source, std::string sKernelName);

	std::shared_ptr<OCL_Buffer> CreateBuffer(size_t size);
};
#endif


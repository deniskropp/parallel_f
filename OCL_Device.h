#ifndef _OCL_DEVICE_H_
#define _OCL_DEVICE_H_

#include <map>
#include <string>

#define CL_TARGET_OPENCL_VERSION 120
#include <CL\opencl.h>

class OCL_Device
{
private:
	cl_platform_id		m_platform_id;
	cl_device_id		m_device_id;
	cl_context			m_context;

	std::string m_sBuildOptions;

	// Uniquely maps a kernel via a program name and a kernel name
	std::map<std::string, std::pair<cl_program, 
		std::map<std::string, cl_kernel> > > m_kernels;
	
	// Maps each buffer to a index
	std::map<int, cl_mem> m_buffers;

	cl_program GetProgramFromFile(std::string filename);
	cl_program GetProgram(std::string source);

public:
	OCL_Device(int iPlatformNum, int iDeviceNum);
	OCL_Device(OCL_Device *main);
	~OCL_Device(void);

	cl_context GetContext();

	void PrintInfo(void);

	cl_command_queue CreateQueue();
	void DestroyQueue(cl_command_queue queue);

	void SetBuildOptions (const char* sBuildOptions);
	cl_kernel GetKernel(std::string filename, std::string sKernelName);
	cl_mem DeviceMalloc(int idx, size_t size);
	void DeviceFree(int idx);
	void CopyBufferToDevice(cl_command_queue queue, void* h_Buffer, int idx, size_t size);
	void CopyBufferToHost  (cl_command_queue queue, void* h_Buffer, int idx, size_t size);
};
#endif


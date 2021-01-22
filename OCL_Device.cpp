#include "OCL_Device.h"
#include "Util.h"

#include "log.hpp"



OCL_Buffer::OCL_Buffer(cl_mem mem)
	:
	mem(mem)
{
}

OCL_Buffer::~OCL_Buffer()
{
	cl_int err;

	err = clReleaseMemObject(mem);
	CHECK_OPENCL_ERROR(err);
}

void OCL_Buffer::CopyBufferToDevice(cl_command_queue queue, void* h_Buffer, size_t size)
{
	LOG_DEBUG("OCL_Buffer::CopyBufferToDevice(%p, %zu)\n", h_Buffer, size);

	cl_int err = clEnqueueWriteBuffer(queue, mem, CL_FALSE, 0, size, h_Buffer, 0, NULL, NULL);
	CHECK_OPENCL_ERROR(err);
}

void OCL_Buffer::CopyBufferToHost(cl_command_queue queue, void* h_Buffer, size_t size)
{
	LOG_DEBUG("OCL_Buffer::CopyBufferToHost(%p, %d, %zu)\n", h_Buffer, size);

	cl_int err = clEnqueueReadBuffer(queue, mem, CL_FALSE, 0, size, h_Buffer, 0, NULL, NULL);
	CHECK_OPENCL_ERROR(err);
}



OCL_Device::OCL_Device(int iPlatformNum, int iDeviceNum)
{
	LOG_DEBUG("OCL_Device::OCL_Device(%d, %d)\n", iPlatformNum, iDeviceNum);

	// For error checking
	cl_int err;

	// Get Platfom Info
	cl_uint iNumPlatforms = 0;
	err = clGetPlatformIDs(NULL, NULL, &iNumPlatforms); 
	CHECK_OPENCL_ERROR(err);

	cl_platform_id* vPlatformIDs = 
		(cl_platform_id *) new cl_platform_id[iNumPlatforms];
	err = clGetPlatformIDs(iNumPlatforms, vPlatformIDs, NULL); 
	CHECK_OPENCL_ERROR(err);
	if (iPlatformNum >= (signed)iNumPlatforms)
	{
		printf("Platform index must me between 0 and %d.\n",iNumPlatforms-1);
		delete[] vPlatformIDs;
		return;
	}
	m_platform_id = vPlatformIDs[iPlatformNum];
	delete[] vPlatformIDs;

	// Get Device Info
	cl_uint iNumDevices = 0;
	err = clGetDeviceIDs(m_platform_id, CL_DEVICE_TYPE_ALL, NULL, NULL, 
		&iNumDevices); 
	CHECK_OPENCL_ERROR(err);

	cl_device_id* vDeviceIDs = (cl_device_id*) new cl_device_id[iNumDevices];	
	err = clGetDeviceIDs(m_platform_id, CL_DEVICE_TYPE_ALL, iNumDevices, 
		vDeviceIDs, &iNumDevices); 
	CHECK_OPENCL_ERROR(err);
	if (iDeviceNum >= (signed)iNumDevices)
	{
		printf("Device index must me between 0 and %d.\n", iNumDevices-1);
		delete[] vDeviceIDs;
		return;
	}
	m_device_id = vDeviceIDs[iDeviceNum];
	delete[] vDeviceIDs;

	cl_context_properties vProprieties[3] = {CL_CONTEXT_PLATFORM, 
		(cl_context_properties)m_platform_id, 0};
	m_context = clCreateContext(vProprieties, 1, &m_device_id, NULL, NULL, 
		&err); 
	CHECK_OPENCL_ERROR(err);
}

OCL_Device::~OCL_Device(void)
{
	LOG_DEBUG("OCL_Device::~OCL_Device()\n");

	// Clean OpenCL Programs and Kernels
	for (std::map<std::string, std::pair<cl_program,
		std::map<std::string, cl_kernel> > >::iterator it = m_kernels.begin();
		it != m_kernels.end(); it++)
	{
		for (std::map<std::string, cl_kernel>::iterator
			it2 = it->second.second.begin();
			it2 != it->second.second.end(); it2++)
		{
			clReleaseKernel(it2->second);
		}
		clReleaseProgram(it->second.first);
	}
}

cl_context OCL_Device::GetContext()
{
	return m_context;
}

void OCL_Device::PrintInfo()
{
	// Printing device and platform information
	printf("Using platform: ");
	PrintPlatformInfo(m_platform_id);
	printf("Using device:   ");
	PrintDeviceInfo(m_device_id);
}

cl_command_queue OCL_Device::CreateQueue()
{
	std::unique_lock<std::mutex> lock(m_queues_lock);

	if (m_queues_list.empty()) {
		cl_int err;

		cl_command_queue queue = clCreateCommandQueue(m_context, m_device_id, NULL, &err);
		CHECK_OPENCL_ERROR(err);

		return queue;
	}

	auto it = m_queues_list.begin();

	cl_command_queue queue = *it;

	m_queues_list.erase(it);

	return queue;
}

void OCL_Device::DestroyQueue(cl_command_queue queue)
{
	std::unique_lock<std::mutex> lock(m_queues_lock);

	if (m_queues_list.size() < 20)
		m_queues_list.push_back(queue);
	else
		clReleaseCommandQueue(queue);
}

cl_program OCL_Device::GetProgramFromFile(std::string filename)
{
	std::string sFile = GetFileContents(filename);

	return GetProgram(sFile);
}

cl_program OCL_Device::GetProgram(std::string source)
{
	std::string sFile = source;
	const char* pFile = sFile.c_str();
	const size_t lFile = sFile.length();

	cl_int err;
	cl_program program = clCreateProgramWithSource(m_context, 1,
		(const char** const)&pFile, &lFile, &err);
	CHECK_OPENCL_ERROR(err);

	err = clBuildProgram(program, 1, &m_device_id, NULL, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		size_t size;
		clGetProgramBuildInfo(program, m_device_id,
			CL_PROGRAM_BUILD_LOG, 0, NULL, &size);
		char* sLog = (char*)malloc(size);
		clGetProgramBuildInfo(program, m_device_id,
			CL_PROGRAM_BUILD_LOG, size, sLog, NULL);

		printf("\n");
		printf("Build Log:\n");
		printf("%s\n", sLog);
		free(sLog);

		CHECK_OPENCL_ERROR(err);
	}

	return program;
}

void OCL_Device::SetBuildOptions(const char* sBuildOptions)
{
	m_sBuildOptions = sBuildOptions;
}

cl_kernel OCL_Device::GetKernel(std::string sProgramName, std::string sKernelName)
{
	std::unique_lock<std::mutex> lock(m_kernels_lock);

	if (m_kernels.find(sProgramName) == m_kernels.end())
	{
		// Build program
		cl_program program = GetProgramFromFile(sProgramName);

		// Add to map
		m_kernels[sProgramName] = std::pair<cl_program, std::map<std::string, cl_kernel> >(program, std::map<std::string, cl_kernel>());
	}

	if (m_kernels[sProgramName].second.find(sKernelName) == m_kernels[sProgramName].second.end()) {
		// Create kernel
		cl_int err;
		cl_kernel kernel = clCreateKernel(m_kernels[sProgramName].first, sKernelName.c_str(), &err);     
		CHECK_OPENCL_ERROR(err);
		
		// Add to map
		m_kernels[sProgramName].second[sKernelName] = kernel;
	}

	return m_kernels[sProgramName].second[sKernelName];
}

cl_kernel OCL_Device::GetKernelFromSource(std::string source, std::string sKernelName)
{
	// Build program
	cl_program program = GetProgram(source);

	// Create kernel
	cl_int err;
	cl_kernel kernel = clCreateKernel(program, sKernelName.c_str(), &err);
	CHECK_OPENCL_ERROR(err);

	return kernel;
}

std::shared_ptr<OCL_Buffer> OCL_Device::CreateBuffer(size_t size)
{
	LOG_DEBUG("OCL_Device::CreateBuffer(%zu)\n", size);

	cl_int err;

	cl_mem mem = clCreateBuffer(m_context, CL_MEM_READ_WRITE, size, NULL, &err);
	CHECK_OPENCL_ERROR(err);

	return std::make_shared<OCL_Buffer>(mem);
}
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <fstream>
#include <string>
#include <map>

#include <Windows.h>

#include "Util.h"


void PrintPlatformInfo(cl_platform_id platformId)
{
	cl_int err = 0;
	// Get Required Size
	size_t length;
	err = clGetPlatformInfo(platformId, CL_PLATFORM_NAME, NULL, NULL, &length);
	CHECK_OPENCL_ERROR(err);
	char* sInfo = new char[length];
 	err = clGetPlatformInfo(platformId, CL_PLATFORM_NAME, length, sInfo, NULL);
	CHECK_OPENCL_ERROR(err);
	printf("%s\n", sInfo);
 	delete[] sInfo;
}

void PrintDeviceInfo(cl_device_id deviceId)
{
	cl_int err = 0;

	// Get Required Size
	size_t length;
	err = clGetDeviceInfo(deviceId, CL_DEVICE_NAME, NULL, NULL, &length);
	// Get actual device name
	char* sInfo = new char[length];
 	err = clGetDeviceInfo(deviceId, CL_DEVICE_NAME, length, sInfo, NULL);
	printf("%s\n", sInfo);
	 
	cl_ulong size;
	err = clGetDeviceInfo(deviceId, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(size),
		&size, NULL);
	CHECK_OPENCL_ERROR(err);
	printf("Total device memory: %llu MB\n", size >> 20);
	
	err = clGetDeviceInfo(deviceId, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(size),
		&size, NULL);
	CHECK_OPENCL_ERROR(err);
	printf("Maximum buffer size: %llu MB\n", size >> 20);
	
 	delete[] sInfo;

}

std::string GetFileContents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in)
  {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  return "";
}

bool FileExists(const char * filename)
{
	FILE* file;
    if (fopen_s(&file, filename, "r"))
    {
        fclose(file);
        return true;
    }
    return false;
}

double GetTime()
{
	LARGE_INTEGER frequency;				// Ticks per Second
    LARGE_INTEGER ticks;					// Ticks
	
    QueryPerformanceFrequency(&frequency);	// Get Ticks per Second
	QueryPerformanceCounter(&ticks);		// Get Start Time

	return double(ticks.QuadPart) / frequency.QuadPart;
}


void CHECK_OPENCL_ERROR(cl_int err)
{
	if (err != CL_SUCCESS)
	{
		switch (err)
		{
		case CL_DEVICE_NOT_FOUND: 
			printf("CL_DEVICE_NOT_FOUND\n"); throw std::runtime_error("OpenCL error"); 
		case CL_DEVICE_NOT_AVAILABLE:	
			printf("CL_DEVICE_NOT_AVAILABLE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_COMPILER_NOT_AVAILABLE:	
			printf("CL_COMPILER_NOT_AVAILABLE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:
			printf("CL_MEM_OBJECT_ALLOCATION_FAILURE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_OUT_OF_RESOURCES:				
			printf("CL_OUT_OF_RESOURCES\n"); throw std::runtime_error("OpenCL error"); 
		case CL_OUT_OF_HOST_MEMORY:				
			printf("CL_OUT_OF_HOST_MEMORY\n"); throw std::runtime_error("OpenCL error"); 
		case CL_PROFILING_INFO_NOT_AVAILABLE:	
			printf("CL_PROFILING_INFO_NOT_AVAILABLE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_MEM_COPY_OVERLAP:				
			printf("CL_MEM_COPY_OVERLAP\n"); throw std::runtime_error("OpenCL error"); 
		case CL_IMAGE_FORMAT_MISMATCH:			
			printf("CL_IMAGE_FORMAT_MISMATCH\n"); throw std::runtime_error("OpenCL error"); 
		case CL_IMAGE_FORMAT_NOT_SUPPORTED:		
			printf("CL_IMAGE_FORMAT_NOT_SUPPORTED\n"); throw std::runtime_error("OpenCL error"); 
		case CL_BUILD_PROGRAM_FAILURE:			
			printf("CL_BUILD_PROGRAM_FAILURE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_MAP_FAILURE:					
			printf("CL_MAP_FAILURE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_MISALIGNED_SUB_BUFFER_OFFSET:	
			printf("CL_MISALIGNED_SUB_BUFFER_OFFSET\n"); throw std::runtime_error("OpenCL error"); 
		case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
			printf("CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST\n"); throw std::runtime_error("OpenCL error");
		case CL_COMPILE_PROGRAM_FAILURE: 
			printf("CL_COMPILE_PROGRAM_FAILURE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_LINKER_NOT_AVAILABLE: 
			printf("CL_LINKER_NOT_AVAILABLE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_LINK_PROGRAM_FAILURE: 
			printf("CL_LINK_PROGRAM_FAILURE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_DEVICE_PARTITION_FAILED: 
			printf("CL_DEVICE_PARTITION_FAILED\n"); throw std::runtime_error("OpenCL error"); 
		case CL_KERNEL_ARG_INFO_NOT_AVAILABLE: 
			printf("CL_KERNEL_ARG_INFO_NOT_AVAILABLE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_VALUE: 
			printf("CL_INVALID_VALUE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_DEVICE_TYPE: 
			printf("CL_INVALID_DEVICE_TYPE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_PLATFORM:
			printf("CL_INVALID_PLATFORM\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_DEVICE:
			printf("CL_INVALID_DEVICE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_CONTEXT:
			printf("CL_INVALID_CONTEXT\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_QUEUE_PROPERTIES:
			printf("CL_INVALID_QUEUE_PROPERTIES\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_COMMAND_QUEUE: 
			printf("CL_INVALID_COMMAND_QUEUE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_HOST_PTR: 
			printf("CL_INVALID_HOST_PTR\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_MEM_OBJECT: 
			printf("CL_INVALID_MEM_OBJECT\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR: 
			printf("CL_INVALID_IMAGE_FORMAT_DESCRIPTOR\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_IMAGE_SIZE:
			printf("CL_INVALID_IMAGE_SIZE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_SAMPLER:
			printf("CL_INVALID_SAMPLER\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_BINARY:
			printf("CL_INVALID_BINARY\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_BUILD_OPTIONS:
			printf("CL_INVALID_BUILD_OPTIONS\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_PROGRAM: 
			printf("CL_INVALID_PROGRAM\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_PROGRAM_EXECUTABLE: 
			printf("CL_INVALID_PROGRAM_EXECUTABLE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_KERNEL_NAME: 
			printf("CL_INVALID_KERNEL_NAME\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_KERNEL_DEFINITION: 
			printf("CL_INVALID_KERNEL_DEFINITION\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_KERNEL: 
			printf("CL_INVALID_KERNEL\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_ARG_INDEX:
			printf("CL_INVALID_ARG_INDEX\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_ARG_VALUE:
			printf("CL_INVALID_ARG_VALUE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_ARG_SIZE: 
			printf("CL_INVALID_ARG_SIZE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_KERNEL_ARGS: 
			printf("CL_INVALID_KERNEL_ARGS\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_WORK_DIMENSION: 
			printf("CL_INVALID_WORK_DIMENSION\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_WORK_GROUP_SIZE: 
			printf("CL_INVALID_WORK_GROUP_SIZE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_WORK_ITEM_SIZE: 
			printf("CL_INVALID_WORK_ITEM_SIZE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_GLOBAL_OFFSET: 
			printf("CL_INVALID_GLOBAL_OFFSET\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_EVENT_WAIT_LIST: 
			printf("CL_INVALID_EVENT_WAIT_LIST\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_EVENT:
			printf("CL_INVALID_EVENT\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_OPERATION:
			printf("CL_INVALID_OPERATION\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_GL_OBJECT: 
			printf("CL_INVALID_GL_OBJECT\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_BUFFER_SIZE: 
			printf("CL_INVALID_BUFFER_SIZE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_MIP_LEVEL: 
			printf("CL_INVALID_MIP_LEVEL\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_GLOBAL_WORK_SIZE: 
			printf("CL_INVALID_GLOBAL_WORK_SIZE\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_PROPERTY:
			printf("CL_INVALID_PROPERTY\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_IMAGE_DESCRIPTOR:
			printf("CL_INVALID_IMAGE_DESCRIPTOR\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_COMPILER_OPTIONS:
			printf("CL_INVALID_COMPILER_OPTIONS\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_LINKER_OPTIONS: 
			printf("CL_INVALID_LINKER_OPTIONS\n"); throw std::runtime_error("OpenCL error"); 
		case CL_INVALID_DEVICE_PARTITION_COUNT: 
			printf("CL_INVALID_DEVICE_PARTITION_COUNT\n"); throw std::runtime_error("OpenCL error"); 
		}
	}
}

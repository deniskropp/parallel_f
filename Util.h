#ifndef _UTIL_H_
#define _UTIL_H_

#include <string>

#define CL_TARGET_OPENCL_VERSION 120
#include <CL\opencl.h>

// ******** General Utility Functions *****************************************

bool FileExists(std::string filename);
std::string GetFileContents(std::string filename);


// ******** OpenCL Utility Functions ******************************************

void PrintPlatformInfo(cl_platform_id);
void PrintDeviceInfo  (cl_device_id);

void CHECK_OPENCL_ERROR(cl_int err);

#endif
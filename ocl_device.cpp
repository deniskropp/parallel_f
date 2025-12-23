#include "ocl_device.hpp"
#include "Util.h"

#include "log.hpp"

using namespace parallel_f::ocl;

ocl_buffer::ocl_buffer(cl_mem mem) : mem(mem) {}

ocl_buffer::~ocl_buffer() {
  cl_int err;

  err = clReleaseMemObject(mem);
  CHECK_OPENCL_ERROR(err);
}

void ocl_buffer::copy_buffer_to_device(cl_command_queue queue, void *h_Buffer,
                                       size_t size) {
  LOG_DEBUG("ocl_buffer::copy_buffer_to_device(%p, %zu)\n", h_Buffer, size);

  cl_int err = clEnqueueWriteBuffer(queue, mem, CL_FALSE, 0, size, h_Buffer, 0,
                                    NULL, NULL);
  CHECK_OPENCL_ERROR(err);
}

void ocl_buffer::copy_buffer_to_host(cl_command_queue queue, void *h_Buffer,
                                     size_t size) {
  LOG_DEBUG("ocl_buffer::copy_buffer_to_host(%p, %d, %zu)\n", h_Buffer, size);

  cl_int err = clEnqueueReadBuffer(queue, mem, CL_FALSE, 0, size, h_Buffer, 0,
                                   NULL, NULL);
  CHECK_OPENCL_ERROR(err);
}

ocl_device::ocl_device(int iPlatformNum, int iDeviceNum) {
  LOG_DEBUG("ocl_device::ocl_device(%d, %d)\n", iPlatformNum, iDeviceNum);

  // For error checking
  cl_int err;

  // Get Platfom Info
  cl_uint iNumPlatforms = 0;
  err = clGetPlatformIDs(NULL, NULL, &iNumPlatforms);
  CHECK_OPENCL_ERROR(err);

  cl_platform_id *vPlatformIDs =
      (cl_platform_id *)new cl_platform_id[iNumPlatforms];
  err = clGetPlatformIDs(iNumPlatforms, vPlatformIDs, NULL);
  CHECK_OPENCL_ERROR(err);
  if (iPlatformNum >= (signed)iNumPlatforms) {
    printf("Platform index must me between 0 and %d.\n", iNumPlatforms - 1);
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

  cl_device_id *vDeviceIDs = (cl_device_id *)new cl_device_id[iNumDevices];
  err = clGetDeviceIDs(m_platform_id, CL_DEVICE_TYPE_ALL, iNumDevices,
                       vDeviceIDs, &iNumDevices);
  CHECK_OPENCL_ERROR(err);
  if (iDeviceNum >= (signed)iNumDevices) {
    printf("Device index must me between 0 and %d.\n", iNumDevices - 1);
    delete[] vDeviceIDs;
    return;
  }
  m_device_id = vDeviceIDs[iDeviceNum];
  delete[] vDeviceIDs;

  cl_context_properties vProprieties[3] = {
      CL_CONTEXT_PLATFORM, (cl_context_properties)m_platform_id, 0};
  m_context = clCreateContext(vProprieties, 1, &m_device_id, NULL, NULL, &err);
  CHECK_OPENCL_ERROR(err);
}

ocl_device::~ocl_device(void) {
  LOG_DEBUG("ocl_device::~ocl_device()\n");

  // Clean OpenCL Programs and Kernels
  for (std::map<std::string,
                std::pair<cl_program, std::map<std::string, cl_kernel>>>::
           iterator it = m_kernels.begin();
       it != m_kernels.end(); it++) {
    for (std::map<std::string, cl_kernel>::iterator it2 =
             it->second.second.begin();
         it2 != it->second.second.end(); it2++) {
      clReleaseKernel(it2->second);
    }
    clReleaseProgram(it->second.first);
  }
}

cl_context ocl_device::get_context() { return m_context; }

void ocl_device::print_info() {
  // Printing device and platform information
  printf("Using platform: ");
  PrintPlatformInfo(m_platform_id);
  printf("Using device:   ");
  PrintDeviceInfo(m_device_id);
}

cl_command_queue ocl_device::create_queue() {
  LOG_DEBUG("ocl_device::create_queue()\n");

  std::unique_lock<std::mutex> lock(m_queues_lock);

  if (m_queues_list.empty()) {
    cl_int err;

    cl_command_queue queue =
        clCreateCommandQueue(m_context, m_device_id, NULL, &err);
    CHECK_OPENCL_ERROR(err);

    return queue;
  }

  auto it = m_queues_list.begin();

  cl_command_queue queue = *it;

  m_queues_list.erase(it);

  return queue;
}

void ocl_device::destroy_queue(cl_command_queue queue) {
  LOG_DEBUG("ocl_device::destroy_queue(%zu)\n", queue);

  std::unique_lock<std::mutex> lock(m_queues_lock);

  if (m_queues_list.size() < 3)
    m_queues_list.push_back(queue);
  else
    clReleaseCommandQueue(queue);
}

cl_program ocl_device::get_program_from_file(std::string filename) {
  std::string sFile = GetFileContents(filename);

  return get_program(sFile);
}

cl_program ocl_device::get_program(std::string source) {
  std::string sFile = source;
  const char *pFile = sFile.c_str();
  const size_t lFile = sFile.length();

  cl_int err;
  cl_program program = clCreateProgramWithSource(
      m_context, 1, (const char **const)&pFile, &lFile, &err);
  CHECK_OPENCL_ERROR(err);

  err = clBuildProgram(program, 1, &m_device_id, NULL, NULL, NULL);
  if (err != CL_SUCCESS) {
    size_t size;
    clGetProgramBuildInfo(program, m_device_id, CL_PROGRAM_BUILD_LOG, 0, NULL,
                          &size);
    char *sLog = (char *)malloc(size);
    clGetProgramBuildInfo(program, m_device_id, CL_PROGRAM_BUILD_LOG, size,
                          sLog, NULL);

    printf("\n");
    printf("Build Log:\n");
    printf("%s\n", sLog);
    free(sLog);

    CHECK_OPENCL_ERROR(err);
  }

  return program;
}

void ocl_device::set_build_options(const char *sBuildOptions) {
  m_sBuildOptions = sBuildOptions;
}

cl_kernel ocl_device::get_kernel(std::string sProgramName,
                                 std::string sKernelName) {
  std::unique_lock<std::mutex> lock(m_kernels_lock);

  if (m_kernels.find(sProgramName) == m_kernels.end()) {
    // Build program
    cl_program program = get_program_from_file(sProgramName);

    // Add to map
    m_kernels[sProgramName] =
        std::pair<cl_program, std::map<std::string, cl_kernel>>(
            program, std::map<std::string, cl_kernel>());
  }

  if (m_kernels[sProgramName].second.find(sKernelName) ==
      m_kernels[sProgramName].second.end()) {
    // Create kernel
    cl_int err;
    cl_kernel kernel = clCreateKernel(m_kernels[sProgramName].first,
                                      sKernelName.c_str(), &err);
    CHECK_OPENCL_ERROR(err);

    // Add to map
    m_kernels[sProgramName].second[sKernelName] = kernel;
  }

  return m_kernels[sProgramName].second[sKernelName];
}

cl_kernel ocl_device::get_kernel_from_source(std::string source,
                                             std::string sKernelName) {
  // Build program
  cl_program program = get_program(source);

  // Create kernel
  cl_int err;
  cl_kernel kernel = clCreateKernel(program, sKernelName.c_str(), &err);
  CHECK_OPENCL_ERROR(err);

  return kernel;
}

std::shared_ptr<ocl_buffer> ocl_device::create_buffer(size_t size) {
  LOG_DEBUG("ocl_device::create_buffer(%zu)\n", size);

  cl_int err;

  cl_mem mem = clCreateBuffer(m_context, CL_MEM_READ_WRITE, size, NULL, &err);
  CHECK_OPENCL_ERROR(err);

  return std::make_shared<ocl_buffer>(mem);
}
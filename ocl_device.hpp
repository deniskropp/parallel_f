#ifndef _OCL_DEVICE_H_
#define _OCL_DEVICE_H_

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/opencl.h>

namespace parallel_f {
namespace ocl {

class ocl_buffer {
private:
  cl_mem mem;

public:
  ocl_buffer(cl_mem mem);
  ~ocl_buffer();

  cl_mem get() const { return mem; }

  void copy_buffer_to_device(cl_command_queue queue, void *h_Buffer,
                             size_t size);
  void copy_buffer_to_host(cl_command_queue queue, void *h_Buffer, size_t size);
};

class ocl_device {
private:
  cl_platform_id m_platform_id;
  cl_device_id m_device_id;
  cl_context m_context;

  std::string m_sBuildOptions;

  std::mutex m_kernels_lock;
  std::mutex m_queues_lock;

  // Uniquely maps a kernel via a program name and a kernel name
  std::map<std::string, std::pair<cl_program, std::map<std::string, cl_kernel>>>
      m_kernels;

  std::list<cl_command_queue> m_queues_list;

  cl_program get_program_from_file(std::string filename);
  cl_program get_program(std::string source);

public:
  ocl_device(int iPlatformNum, int iDeviceNum);
  ~ocl_device(void);

  cl_context get_context();

  void print_info(void);

  cl_command_queue create_queue();
  void destroy_queue(cl_command_queue queue);

  void set_build_options(const char *sBuildOptions);
  cl_kernel get_kernel(std::string filename, std::string sKernelName);
  cl_kernel get_kernel_from_source(std::string source, std::string sKernelName);

  std::shared_ptr<ocl_buffer> create_buffer(size_t size);
};

} // namespace ocl

using ocl::ocl_buffer;
using ocl::ocl_device;

} // namespace parallel_f
#endif

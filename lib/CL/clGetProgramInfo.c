/* OpenCL runtime library: clGetProgramInfo()

   Copyright (c) 2011 Erik Schnetter
   
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <string.h>
#include "pocl_llvm.h"
#include "pocl_util.h"

CL_API_ENTRY cl_int CL_API_CALL
POname(clGetProgramInfo)(cl_program program,
                 cl_program_info param_name,
                 size_t param_value_size,
                 void *param_value,
                 size_t *param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
  unsigned i;

  POCL_RETURN_ERROR_COND((program == NULL), CL_INVALID_PROGRAM);

  switch (param_name)
  {
  case CL_PROGRAM_REFERENCE_COUNT:
    POCL_RETURN_GETINFO(cl_uint, (cl_uint)program->pocl_refcount);
  case CL_PROGRAM_CONTEXT:
    POCL_RETURN_GETINFO(cl_context, program->context);

  case CL_PROGRAM_SOURCE:
    {
      const char *source = program->source;
      if (source == NULL)
        source = "";

      POCL_RETURN_GETINFO_STR(source);
    }
    
  case CL_PROGRAM_BINARY_SIZES:
    {
      size_t const value_size = sizeof(size_t) * program->num_devices;
      if (param_value)
        pocl_llvm_update_binaries (program);
      POCL_RETURN_GETINFO_SIZE(value_size, program->binary_sizes);
    }

  case CL_PROGRAM_BINARIES:
    {
      size_t const value_size = sizeof(unsigned char *) * program->num_devices;
      if (param_value)
      {
        if (param_value_size < value_size) return CL_INVALID_VALUE;
        for (i = 0; i < program->num_devices; ++i)
          {
            unsigned char **target = (unsigned char**) param_value;
            if (target[i] == NULL) continue;
            memcpy (target[i], program->binaries[i], program->binary_sizes[i]);
          }
      }
      if (param_value_size_ret)
        *param_value_size_ret = value_size;
      return CL_SUCCESS;
    }
  case CL_PROGRAM_NUM_DEVICES:
    POCL_RETURN_GETINFO(cl_uint, program->num_devices);

  case CL_PROGRAM_DEVICES:
    {
      size_t const value_size = sizeof(cl_device_id) * program->num_devices;
      if (param_value)
      {
        if (param_value_size < value_size) return CL_INVALID_VALUE;
        for (i = 0; i < program->num_devices; ++i)
          {           
            cl_device_id *devices = (cl_device_id*) param_value;
            devices[i] = program->context->devices[i];
          }
      }
      if (param_value_size_ret)
        *param_value_size_ret = value_size;
      return CL_SUCCESS;
    }
  case CL_PROGRAM_KERNEL_NAMES:
    {
      const char *kernel_names[32];
      unsigned num_kernels = 0;
      size_t size = 0;
      num_kernels = pocl_llvm_get_kernel_names(program, kernel_names, 32);
      for (i = 0; i < num_kernels; ++i)
        {
          if (size + strlen (kernel_names[i]) + 1 >= param_value_size)
            break;
          size += strlen (kernel_names[i]) + 1;
          
          if (i == 0)
            memcpy (param_value, kernel_names[i], strlen(kernel_names[i])+1);
          else
            strcat((char*)param_value, kernel_names[i]);
          if (i != num_kernels - 1)
            strcat ((char*)param_value, ";");
        }

      if (param_value_size_ret)
        *param_value_size_ret = size;      
      return CL_SUCCESS;
    }
  default:
    break;
  }
  
  char error_str[64];
  sprintf(error_str, "clGetProgramInfo: %X", param_name);
  POCL_ABORT_UNIMPLEMENTED(error_str);
  return CL_INVALID_VALUE;
}
POsym(clGetProgramInfo)

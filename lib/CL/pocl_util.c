/* OpenCL runtime library: pocl_util utility functions

   Copyright (c) 2012 Pekka Jääskeläinen / Tampere University of Technology
   
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#ifndef _MSC_VER
#  include <dirent.h>
#  include <unistd.h>
#  include <utime.h>
#else
#  include "vccompat.hpp"
#endif

#include "pocl_util.h"
#include "utlist.h"
#include "common.h"
#include "pocl_mem_management.h"
#include "pocl_runtime_config.h"


#define CACHE_DIR_PATH_CHARS 512

struct list_item;

typedef struct list_item
{
  void *value;
  struct list_item *next;
} list_item;

void 
pocl_remove_directory (const char *path_name)
{
  int str_size = 10 + strlen(path_name) + 1;
  char *cmd = (char*)malloc(str_size);
  snprintf(cmd, str_size, "rm -fr '%s'", path_name);
  system(cmd);
  POCL_MEM_FREE(cmd);
}

void
pocl_remove_file (const char *file_path)
{
  int str_size = 10 + strlen(file_path) + 1;
  char *cmd = (char*)malloc(str_size);
  snprintf(cmd, str_size, "rm -f '%s'", file_path);
  system(cmd);
  POCL_MEM_FREE(cmd);
}

void
pocl_make_directory (const char *path_name)
{
  int str_size = 12 + strlen(path_name) + 1;
  char *cmd = (char*)malloc(str_size);
  snprintf(cmd, str_size, "mkdir -p '%s'", path_name);
  system(cmd);
  POCL_MEM_FREE(cmd);
}

void
pocl_create_or_append_file (const char *file_name, const char *content)
{
  FILE *fp = fopen(file_name, "a");
  if ((fp == NULL) || (content == NULL))
    return;

  fprintf(fp, "%s", content);

  fclose(fp);
}

int
pocl_read_text_file (const char* file_name, char** content_dptr)
{
  FILE *fp;
  struct stat st;
  int file_size;

  stat(file_name, &st);
  file_size = (int)st.st_size;

  fp = fopen(file_name, "r");
  if (fp == NULL)
    return 0;

  *content_dptr = (char*) malloc((file_size + 1) * sizeof(char));
  if (!(*content_dptr)) return 0;

  int read = fread(*content_dptr, sizeof(char), file_size, fp);
  (*content_dptr)[file_size] = '\0';

  return read;
}

char*
pocl_create_program_cache_dir(cl_program program)
{
  char *tmp_path = NULL, *cache_path = NULL;
  char hash_str[SHA1_DIGEST_SIZE * 2 + 1];
  int i;

  for (i = 0; i < SHA1_DIGEST_SIZE; i++)
    sprintf(&hash_str[i*2], "%02x", (unsigned int) program->build_hash[i]);

  cache_path = (char*)malloc(CACHE_DIR_PATH_CHARS);

  tmp_path = getenv("POCL_CACHE_DIR");
  if (tmp_path && (access(tmp_path, W_OK) == 0))
    {
      snprintf(cache_path, CACHE_DIR_PATH_CHARS, "%s/%s", tmp_path, hash_str);
    }
  else
    {
#ifdef POCL_ANDROID
      snprintf(cache_path, CACHE_DIR_PATH_CHARS,
                  "/data/data/%s/cache/", pocl_get_process_name());

      if (access(cache_path, W_OK) == 0)
        strcat(cache_path, hash_str);
      else
        snprintf(cache_path, CACHE_DIR_PATH_CHARS, "/sdcard/pocl/kcache/%s", hash_str);
#else
      tmp_path = getenv("HOME");

      if (tmp_path)
        snprintf(cache_path, CACHE_DIR_PATH_CHARS, "%s/.pocl/kcache/%s", tmp_path, hash_str);
      else
        snprintf(cache_path, CACHE_DIR_PATH_CHARS, "/tmp/pocl/kcache/%s", hash_str);
#endif
    }

  if (access(cache_path, F_OK) != 0)
    pocl_make_directory(cache_path);

    return cache_path;
}

uint32_t
byteswap_uint32_t (uint32_t word, char should_swap) 
{
    union word_union 
    {
        uint32_t full_word;
        unsigned char bytes[4];
    } old, neww;
    if (!should_swap) return word;

    old.full_word = word;
    neww.bytes[0] = old.bytes[3];
    neww.bytes[1] = old.bytes[2];
    neww.bytes[2] = old.bytes[1];
    neww.bytes[3] = old.bytes[0];
    return neww.full_word;
}

float
byteswap_float (float word, char should_swap) 
{
    union word_union 
    {
        float full_word;
        unsigned char bytes[4];
    } old, neww;
    if (!should_swap) return word;

    old.full_word = word;
    neww.bytes[0] = old.bytes[3];
    neww.bytes[1] = old.bytes[2];
    neww.bytes[2] = old.bytes[1];
    neww.bytes[3] = old.bytes[0];
    return neww.full_word;
}

size_t
pocl_size_ceil2(size_t x) {
  /* Rounds up to the next highest power of two without branching and
   * is as fast as a BSR instruction on x86, see:
   *
   * http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
   */
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
#if SIZE_MAX > 0xFFFFFFFF
  x |= x >> 32;
#endif
  return ++x;
}

#ifndef HAVE_ALIGNED_ALLOC
void *
pocl_aligned_malloc (size_t alignment, size_t size)
{
# ifdef HAVE_POSIX_MEMALIGN
  
  /* make sure that size is a multiple of alignment, as posix_memalign
   * does not perform this test, whereas aligned_alloc does */
  if ((size & (alignment - 1)) != 0)
    {
      errno = EINVAL;
      return NULL;
    }

  /* posix_memalign requires alignment to be at least sizeof(void *) */
  if (alignment < sizeof(void *))
    alignment = sizeof(void* );

  void* result;
  
  result = pocl_memalign_alloc(alignment, size);
  if (result == NULL)
    {
      errno = -1;
      return NULL;
    }

  return result;

# else
  
  /* allow zero-sized allocations, force alignment to 1 */
  if (!size)
    alignment = 1;

  /* make sure alignment is a non-zero power of two and that
   * size is a multiple of alignment */
  size_t mask = alignment - 1;
  if (!alignment || ((alignment & mask) != 0) || ((size & mask) != 0))
    {
      errno = EINVAL;
      return NULL;
    }

  /* allocate memory plus space for alignment header */
  uintptr_t address = (uintptr_t)malloc(size + mask + sizeof(void *));
  if (!address)
    return NULL;

  /* align the address, and store original pointer for future use
   * with free in the preceeding bytes */
  uintptr_t aligned_address = (address + mask + sizeof(void *)) & ~mask;
  void** address_ptr = (void **)(aligned_address - sizeof(void *));
  *address_ptr = (void *)address;
  return (void *)aligned_address;

#endif
}
#endif

#if !defined HAVE_ALIGNED_ALLOC && !defined HAVE_POSIX_MEMALIGN
void
pocl_aligned_free (void *ptr)
{
  /* extract pointer from original allocation and free it */
  if (ptr)
    free(*(void **)((uintptr_t)ptr - sizeof(void *)));
}
#endif

cl_int pocl_create_event (cl_event *event, cl_command_queue command_queue, 
                          cl_command_type command_type)
{
  if (event != NULL)
    {
      *event = pocl_mem_manager_new_event ();
      if (event == NULL)
        return CL_OUT_OF_HOST_MEMORY;
      
      (*event)->queue = command_queue;
      POname(clRetainCommandQueue) (command_queue);
      (*event)->command_type = command_type;
      (*event)->callback_list = NULL;
      (*event)->implicit_event = 0;
      (*event)->next = NULL;
    }
  return CL_SUCCESS;
}

cl_int pocl_create_command (_cl_command_node **cmd,
                            cl_command_queue command_queue, 
                            cl_command_type command_type, cl_event *event_p, 
                            cl_int num_events, const cl_event *wait_list)
{
  int i;
  int err;
  cl_event *event = NULL;

  if ((wait_list == NULL && num_events != 0) ||
      (wait_list != NULL && num_events == 0))
    return CL_INVALID_EVENT_WAIT_LIST;
  
  for (i = 0; i < num_events; ++i)
    {
      if (wait_list[i] == NULL)
        return CL_INVALID_EVENT_WAIT_LIST;
    }
  
  *cmd = pocl_mem_manager_new_command ();

  if (*cmd == NULL)
    return CL_OUT_OF_HOST_MEMORY;
  
  /* if user does not provide event pointer, create event anyway */
  event = &((*cmd)->event);
  err = pocl_create_event(event, command_queue, command_type);
  if (err != CL_SUCCESS)
    {
      POCL_MEM_FREE(*cmd);
      return err;
    }
  if (event_p)
    *event_p = *event;
  else
    (*event)->implicit_event = 1;
  
  /* if in-order command queue and queue is not empty, add event from 
     previous command to new commands event_waitlist */
  if (!(command_queue->properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) 
      && command_queue->root != NULL)
    {
      _cl_command_node *prev_command;
      for (prev_command = command_queue->root; prev_command->next != NULL;
           prev_command = prev_command->next){}
      //printf("create_command: prev_com=%d prev_com->event = %d \n",prev_command, prev_command->event);
      cl_event *new_wl = (cl_event*)malloc ((num_events +1)*sizeof (cl_event));
      for (i = 0; i < num_events; ++i)
        {
          new_wl[i] = wait_list[i];
        }
      new_wl[i] = prev_command->event;
      (*cmd)->event_wait_list = new_wl;
      (*cmd)->num_events_in_wait_list = num_events + 1;
      for (i = 0; i < num_events + 1; ++i)
        {
          //printf("create-command: new_wl[%i]=%d\n", i, new_wl[i]);
        }
    }
  else
    {
      (*cmd)->event_wait_list = wait_list;  
      (*cmd)->num_events_in_wait_list = num_events;
    }
  (*cmd)->type = command_type;
  (*cmd)->next = NULL;
  (*cmd)->device = command_queue->device;

  //printf("create_command (end): event=%d new_event=%d cmd->event=%d cmd=%d\n", event, new_event, (*cmd)->event, *cmd);
  

  return CL_SUCCESS;
}

void pocl_command_enqueue (cl_command_queue command_queue,
                          _cl_command_node *node)
{
  POCL_LOCK_OBJ(command_queue);
  LL_APPEND (command_queue->root, node);
  POCL_UNLOCK_OBJ(command_queue);
  #ifdef POCL_DEBUG_BUILD
  if (pocl_is_option_set("POCL_IMPLICIT_FINISH"))
    POclFinish (command_queue);
  #endif
  POCL_UPDATE_EVENT_QUEUED (&node->event, command_queue);

}

char* pocl_get_process_name ()
{
  char tmpStr[64], cmdline[512], *processName = NULL;
  FILE *statusFile;
  int len, i, begin;

  snprintf(tmpStr, 64, "/proc/%d/cmdline", getpid());
  statusFile = fopen(tmpStr, "r");
  if (statusFile == NULL)
    return NULL;

  if (fgets(cmdline, 511, statusFile) != NULL)
    {
      len = strlen(cmdline);
      begin = 0;
      for (i=len-1; i>=0; i--)     /* Extract program-name after last '/' */
        {
          if (cmdline[i] == '/')
            {
              begin = i + 1;
              break;
            }
        }
      processName = strdup(cmdline + begin);
    }

  fclose(statusFile);
  return processName;
}

static int cache_lock_initialized = 0;
static pocl_lock_t cache_lock = POCL_LOCK_INITIALIZER;

void
pocl_check_and_invalidate_cache (cl_program program,
                  int device_i, const char* device_tmpdir)
{
  int cache_dirty = 0;
  char *content = NULL, *s_ptr, *ss_ptr;
  int read = 0;

  POCL_LOCK(cache_lock);

  if (!pocl_get_bool_option("POCL_KERNEL_CACHE", POCL_BUILD_KERNEL_CACHE))
    {
      cache_dirty = 1;
      goto bottom;
    }

  /* If program contains "#include", disable caching
     Included headers might get modified, force recompilation in all the cases
     Yes, this is a very dirty way to find "# include"
     but we can live with this for now
   */
  if (!pocl_get_bool_option("POCL_KERNEL_CACHE_IGNORE_INCLUDES", 0) &&
      program->source)
    {
      for (s_ptr = program->source; (*s_ptr); s_ptr++)
        {
          if ((*s_ptr) == '#')
            {
              /* Skip all the white-spaces between # & include */
              for (ss_ptr = s_ptr+1; *ss_ptr == ' '; ss_ptr++) ;
              
              if (strncmp(ss_ptr, "include", 7) == 0)
                cache_dirty = 1;
            }
        }
    }

  bottom:
  if (cache_dirty)
    {
      pocl_remove_directory(device_tmpdir);
      mkdir(device_tmpdir, S_IRWXU);
    }

  POCL_UNLOCK(cache_lock);
}

void pocl_touch_file(const char* file_name)
{
  struct stat file_stat;
  struct utimbuf new_time;

  if (access(file_name, F_OK) != 0)
    {
      FILE *fp = fopen(file_name, "w");
      fclose(fp);
    }

  stat(file_name, &file_stat);

  new_time.actime = file_stat.st_atime;
  new_time.modtime = time(NULL);        /* set mtime to current time */
  utime(file_name, &new_time);
}


int pocl_buffer_boundcheck(cl_mem buffer, size_t offset, size_t size) {
  POCL_RETURN_ERROR_ON((offset > buffer->size), CL_INVALID_VALUE,
            "offset(%zu) > buffer->size(%zu)", offset, buffer->size)
  POCL_RETURN_ERROR_ON((size > buffer->size), CL_INVALID_VALUE,
            "size(%zu) > buffer->size(%zu)", size, buffer->size)
  POCL_RETURN_ERROR_ON((offset + size > buffer->size), CL_INVALID_VALUE,
            "offset + size (%zu) > buffer->size(%zu)", (offset+size), buffer->size)
  return CL_SUCCESS;
}

int pocl_buffer_boundcheck_3d(const size_t buffer_size,
                              const size_t *origin,
                              const size_t *region,
                              size_t *row_pitch,
                              size_t *slice_pitch,
                              const char* prefix)
{
  size_t rp = *row_pitch;
  size_t sp = *slice_pitch;

  /* CL_INVALID_VALUE if row_pitch is not 0 and is less than region[0]. */
  POCL_RETURN_ERROR_ON((rp != 0 && rp<region[0]),
    CL_INVALID_VALUE, "%srow_pitch is not 0 and is less than region[0]\n", prefix);

  if (rp == 0) rp = region[0];

  /* CL_INVALID_VALUE if slice_pitch is not 0 and is less than region[1] * row_pitch
   * or if slice_pitch is not 0 and is not a multiple of row_pitch.
   */
  POCL_RETURN_ERROR_ON((sp != 0 && sp < (region[1] * rp)),
    CL_INVALID_VALUE, "%sslice_pitch is not 0 and is less than "
      "region[1] * %srow_pitch\n", prefix, prefix);
  POCL_RETURN_ERROR_ON((sp != 0 && (sp % rp != 0)),
    CL_INVALID_VALUE, "%sslice_pitch is not 0 and is not a multiple "
      "of %srow_pitch\n", prefix, prefix);

  if (sp == 0) sp = region[1] * rp;

  *row_pitch = rp;
  *slice_pitch = sp;

  size_t byte_offset_begin = origin[2] * sp +
               origin[1] * rp +
               origin[0];

  size_t byte_offset_end = origin[0] + region[0]-1 +
       rp * (origin[1] + region[1]-1) +
       sp * (origin[2] + region[2]-1);


  POCL_RETURN_ERROR_ON((byte_offset_begin > buffer_size), CL_INVALID_VALUE,
            "%sorigin is outside the %sbuffer", prefix, prefix);
  POCL_RETURN_ERROR_ON((byte_offset_end > buffer_size), CL_INVALID_VALUE,
            "%sorigin+region is outside the %sbuffer", prefix, prefix);
  return CL_SUCCESS;
}



int pocl_buffers_boundcheck(cl_mem src_buffer,
                            cl_mem dst_buffer,
                            size_t src_offset,
                            size_t dst_offset,
                            size_t size) {
  POCL_RETURN_ERROR_ON((src_offset > src_buffer->size), CL_INVALID_VALUE,
            "src_offset(%zu) > src_buffer->size(%zu)", src_offset, src_buffer->size)
  POCL_RETURN_ERROR_ON((size > src_buffer->size), CL_INVALID_VALUE,
            "size(%zu) > src_buffer->size(%zu)", size, src_buffer->size)
  POCL_RETURN_ERROR_ON((src_offset + size > src_buffer->size), CL_INVALID_VALUE,
            "src_offset + size (%zu) > src_buffer->size(%zu)", (src_offset+size), src_buffer->size)

  POCL_RETURN_ERROR_ON((dst_offset > dst_buffer->size), CL_INVALID_VALUE,
            "dst_offset(%zu) > dst_buffer->size(%zu)", dst_offset, dst_buffer->size)
  POCL_RETURN_ERROR_ON((size > dst_buffer->size), CL_INVALID_VALUE,
            "size(%zu) > dst_buffer->size(%zu)", size, dst_buffer->size)
  POCL_RETURN_ERROR_ON((dst_offset + size > dst_buffer->size), CL_INVALID_VALUE,
            "dst_offset + size (%zu) > dst_buffer->size(%zu)", (dst_offset+size), dst_buffer->size)
  return CL_SUCCESS;
}

int pocl_buffers_overlap(cl_mem src_buffer,
                         cl_mem dst_buffer,
                         size_t src_offset,
                         size_t dst_offset,
                         size_t size) {
  /* The regions overlap if src_offset ≤ to dst_offset ≤ to src_offset + size - 1,
   * or if dst_offset ≤ to src_offset ≤ to dst_offset + size - 1.
   */
  if (src_buffer == dst_buffer) {
    POCL_RETURN_ERROR_ON(((src_offset <= dst_offset) && (dst_offset <=
      (src_offset + size - 1))), CL_MEM_COPY_OVERLAP, "dst_offset lies inside \
      the src region and the src_buffer == dst_buffer")
    POCL_RETURN_ERROR_ON(((dst_offset <= src_offset) && (src_offset <=
      (dst_offset + size - 1))), CL_MEM_COPY_OVERLAP, "src_offset lies inside \
      the dst region and the src_buffer == dst_buffer")
  }

  /* sub buffers overlap check  */
  if (src_buffer->parent && dst_buffer->parent &&
        (src_buffer->parent == dst_buffer->parent)) {
      src_offset = (char*)src_buffer->mem_host_ptr - (char*)src_buffer->parent->mem_host_ptr +
        src_offset;
      dst_offset = (char*)dst_buffer->mem_host_ptr - (char*)dst_buffer->parent->mem_host_ptr +
        dst_offset;

    POCL_RETURN_ERROR_ON(((src_offset <= dst_offset) && (dst_offset <=
      (src_offset + size - 1))), CL_MEM_COPY_OVERLAP, "dst_offset lies inside \
      the src region and src_buffer + dst_buffer are subbuffers of the same buffer")
    POCL_RETURN_ERROR_ON(((dst_offset <= src_offset) && (src_offset <=
      (dst_offset + size - 1))), CL_MEM_COPY_OVERLAP, "src_offset lies inside \
      the dst region and src_buffer + dst_buffer are subbuffers of the same buffer")

  }

  return CL_SUCCESS;
}

/*
 * Copyright (c) 2011 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and /or associated documentation files (the "Materials "), to deal in the Materials
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Materials, and to permit persons to
 * whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE MATERIALS OR THE USE OR OTHER DEALINGS IN
 * THE MATERIALS.
 */

int
check_copy_overlap(const size_t src_offset[3],
                   const size_t dst_offset[3],
                   const size_t region[3],
                   const size_t row_pitch, const size_t slice_pitch)
{
  const size_t src_min[] = {src_offset[0], src_offset[1], src_offset[2]};
  const size_t src_max[] = {src_offset[0] + region[0],
                            src_offset[1] + region[1],
                            src_offset[2] + region[2]};
  const size_t dst_min[] = {dst_offset[0], dst_offset[1], dst_offset[2]};
  const size_t dst_max[] = {dst_offset[0] + region[0],
                            dst_offset[1] + region[1],
                            dst_offset[2] + region[2]};
  int overlap = 1;
  unsigned i;
  for (i=0; i != 3; ++i)
  {
    overlap = overlap && (src_min[i] < dst_max[i])
                      && (src_max[i] > dst_min[i]);
  }

  size_t dst_start =  dst_offset[2] * slice_pitch +
                      dst_offset[1] * row_pitch + dst_offset[0];
  size_t dst_end = dst_start + (region[2] * slice_pitch +
                                region[1] * row_pitch + region[0]);
  size_t src_start =  src_offset[2] * slice_pitch +
                      src_offset[1] * row_pitch + src_offset[0];
  size_t src_end = src_start + (region[2] * slice_pitch +
                                region[1] * row_pitch + region[0]);

  if (!overlap)
  {
    size_t delta_src_x = (src_offset[0] + region[0] > row_pitch) ?
                          src_offset[0] + region[0] - row_pitch : 0;
    size_t delta_dst_x = (dst_offset[0] + region[0] > row_pitch) ?
                          dst_offset[0] + region[0] - row_pitch : 0;
    if ( (delta_src_x > 0 && delta_src_x > dst_offset[0]) ||
          (delta_dst_x > 0 && delta_dst_x > src_offset[0]) )
      {
        if ( (src_start <= dst_start && dst_start < src_end) ||
          (dst_start <= src_start && src_start < dst_end) )
          overlap = 1;
      }

    if (region[2] > 1)
    {
      size_t src_height = slice_pitch / row_pitch;
      size_t dst_height = slice_pitch / row_pitch;

      size_t delta_src_y = (src_offset[1] + region[1] > src_height) ?
                            src_offset[1] + region[1] - src_height : 0;
      size_t delta_dst_y = (dst_offset[1] + region[1] > dst_height) ?
                            dst_offset[1] + region[1] - dst_height : 0;

      if ( (delta_src_y > 0 && delta_src_y > dst_offset[1]) ||
            (delta_dst_y > 0 && delta_dst_y > src_offset[1]) )
      {
        if ( (src_start <= dst_start && dst_start < src_end) ||
              (dst_start <= src_start && src_start < dst_end) )
              overlap = 1;
      }
    }
  }

  return overlap;
}

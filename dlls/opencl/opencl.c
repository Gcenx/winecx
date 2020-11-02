/*
 * OpenCL.dll proxy for native OpenCL implementation.
 *
 * Copyright 2010 Peter Urbanec
 * Copyright 2019 Conor McCarthy for Codeweavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"
#include "wine/library.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(opencl);

#if defined(HAVE_CL_CL_H)
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include <CL/cl.h>
#elif defined(HAVE_OPENCL_OPENCL_H)
#include <OpenCL/opencl.h>
#endif

/* TODO: Figure out how to provide GL context sharing before enabling OpenGL */
#define OPENCL_WITH_GL 0

typedef INT_PTR wine_cl_context_properties;


/*---------------------------------------------------------------*/
/* 32-on-64 wrapping and data/pointer mirroring helpers */

static inline void convert_size_t_array(size_t *dest, const SIZE_T *src, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i)
        dest[i] = src[i];
}

#ifdef __i386_on_x86_64__

typedef struct
{
    void *HOSTPTR handle;
    /* Querying the value of CL_CONTEXT_REFERENCE_COUNT to check if a handle can be freed
     * is not thread safe and may query a destroyed context. We need our own ref count. */
    LONG volatile ref_count;
} *wine_cl_handle;

typedef wine_cl_handle wine_cl_platform_id;
typedef wine_cl_handle wine_cl_device_id;
typedef wine_cl_handle wine_cl_context;
typedef wine_cl_handle wine_cl_command_queue;
typedef wine_cl_handle wine_cl_mem;
typedef wine_cl_handle wine_cl_program;
typedef wine_cl_handle wine_cl_kernel;
typedef wine_cl_handle wine_cl_event;
typedef wine_cl_handle wine_cl_sampler;

#define WINE_CL_LOCAL_EVENTS 64

static inline wine_cl_handle cl_wrap_handle(void *HOSTPTR handle)
{
    wine_cl_handle ret = NULL;
    if (handle && (ret = heap_alloc(sizeof(*ret))))
    {
        ret->handle = handle;
        ret->ref_count = 1;
    }
    return ret;
}

static inline void cl_wrap_handle_indirect(void *HOSTPTR handle, wine_cl_handle *ret)
{
    if (ret)
        *ret = cl_wrap_handle(handle);
}

static inline void *HOSTPTR cl_get_handle(wine_cl_handle handle)
{
    return handle->handle;
}

static inline void cl_retain_handle(wine_cl_handle handle)
{
    InterlockedIncrement(&handle->ref_count);
}

static inline void cl_release_handle(wine_cl_handle handle)
{
    if (!InterlockedDecrement(&handle->ref_count))
        heap_free(handle);
}

static inline size_t *mirror_size_t_array(const SIZE_T *array, size_t count)
{
    size_t *ret = NULL;
    if (array && (ret = heap_alloc(count * sizeof(size_t))))
        convert_size_t_array(ret, array, count);
    return ret;
}

static inline void *HOSTPTR *mirror_handle_array(const wine_cl_handle *handles, size_t count)
{
    void *HOSTPTR *ret = NULL;
    if (handles && (ret = heap_alloc(count * sizeof(void *HOSTPTR))))
    {
      size_t i;
      for (i = 0; i < count; ++i)
          ret[i] = cl_get_handle(handles[i]);
    }
    return ret;
}

static inline cl_device_id *mirror_device_array(const wine_cl_device_id *devices, size_t count)
{
    return (cl_device_id *)mirror_handle_array(devices, count);
}

static inline cl_event *mirror_event_array(const wine_cl_event *events, size_t count)
{
    return (cl_event *)mirror_handle_array(events, count);
}

static inline char mirroring_succeeded(const wine_cl_handle *handles, const void *HOSTPTR mirror)
{
    return !handles || mirror;
}

static inline void *mirror_string_array(const char **pointers, size_t count)
{
    const char *HOSTPTR *ret = NULL;
    if (pointers && (ret = heap_alloc(count * sizeof(const char *HOSTPTR))))
    {
      size_t i;
      for (i = 0; i < count; ++i)
          ret[i] = pointers[i];
    }
    return ret;
}

static inline void *HOSTPTR *alloc_temp_output_handles(wine_cl_handle *dest, cl_uint count)
{
    (void)dest;
    return heap_alloc(count * sizeof(void * HOSTPTR));
}

static inline cl_int convert_output_handles(cl_int ret, wine_cl_handle *dest, void *HOSTPTR *src, cl_uint *count)
{
    size_t i;
    for (i = 0; i < *count; ++i)
        dest[i] = cl_wrap_handle(src[i]);
    heap_free(src);
    return ret;
}

static inline char *mirror_callback_string(const char *HOSTPTR message)
{
    return message ? heap_strdup(message) : NULL;
}

static inline char *mirror_callback_data(const void *HOSTPTR private, size_t cb)
{
    void *data = NULL;
    if (private && (data = heap_alloc(cb)))
        memcpy(data, private, cb);
    return data;
}

static inline void cl_temp_free(void *HOSTPTR buffer)
{
    heap_free(buffer);
}

#else

typedef void *wine_cl_handle;
typedef cl_platform_id wine_cl_platform_id;
typedef cl_device_id wine_cl_device_id;
typedef cl_context wine_cl_context;
typedef cl_command_queue wine_cl_command_queue;
typedef cl_mem wine_cl_mem;
typedef cl_program wine_cl_program;
typedef cl_kernel wine_cl_kernel;
typedef cl_event wine_cl_event;
typedef cl_sampler wine_cl_sampler;

static inline wine_cl_handle cl_wrap_handle(void *HOSTPTR handle)
{
    return handle;
}

static inline void cl_wrap_handle_indirect(void *HOSTPTR handle, void *ret)
{
    if (ret)
        *(void**)ret = handle;
}

static inline void *HOSTPTR cl_get_handle(wine_cl_handle handle)
{
    return handle;
}

static inline void cl_retain_handle(wine_cl_handle handle)
{
    (void)handle;
}

static inline void cl_release_handle(wine_cl_handle handle)
{
    (void)handle;
}

static inline size_t *mirror_size_t_array(const SIZE_T *array, size_t count)
{
    (void)count;
    return (size_t *)array;
}

static inline cl_device_id *mirror_device_array(const wine_cl_device_id *devices, size_t count)
{
    (void)count;
    return (cl_device_id *)devices;
}

static inline cl_event *mirror_event_array(const wine_cl_event *events, size_t count)
{
    (void)count;
    return (cl_event *)events;
}

static inline char mirroring_succeeded(const void *handles, const void *mirror)
{
    (void)handles; (void)mirror;
    return 1;
}

static inline void *mirror_string_array(const char **pointers, size_t count)
{
    (void)count;
    return pointers;
}

static inline void *HOSTPTR *alloc_temp_output_handles(void *dest, cl_uint count)
{
    (void)count;
    return dest;
}

static inline cl_int convert_output_handles(cl_int ret, void *dest, void *src, cl_uint *count)
{
    (void)dest; (void)src; (void)count;
    return ret;
}

static inline char *mirror_callback_string(const char *HOSTPTR message)
{
    return (char *)message;
}

static inline char *mirror_callback_data(const void *HOSTPTR private, size_t cb)
{
    (void)cb;
    return (char *)private;
}

static inline void cl_temp_free(void *HOSTPTR buffer)
{
    (void)buffer;
}

#endif

static inline void size_t_return_indirect(size_t ret, SIZE_T *dest)
{
    if (dest)
        *dest = (SIZE_T)ret;
}

/*---------------------------------------------------------------*/
/* Platform API */

cl_int WINAPI wine_clGetPlatformIDs(cl_uint num_entries, wine_cl_platform_id *platforms, cl_uint *num_platforms)
{
    void *HOSTPTR *ids;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("(%d, %p, %p)\n", num_entries, platforms, num_platforms);
    if ((ids = alloc_temp_output_handles(platforms, num_entries)))
        ret = convert_output_handles(clGetPlatformIDs(num_entries, (cl_platform_id *)ids, num_platforms), platforms, ids, num_platforms);
    TRACE("(%d, %p, %p)=%d\n", num_entries, platforms, num_platforms, ret);
    return ret;
}

cl_int WINAPI wine_clGetPlatformInfo(wine_cl_platform_id platform, cl_platform_info param_name,
                                     SIZE_T param_value_size, void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("(%p, 0x%x, %ld, %p, %p)\n", platform, param_name, param_value_size, param_value, param_value_size_ret);

    /* Hide all extensions.
     * TODO: Add individual extension support as needed.
     */
    if (param_name == CL_PLATFORM_EXTENSIONS)
    {
        ret = CL_INVALID_VALUE;

        if (param_value && param_value_size > 0)
        {
            char *exts = (char *) param_value;
            exts[0] = '\0';
            ret = CL_SUCCESS;
        }

        if (param_value_size_ret)
        {
            *param_value_size_ret = 1;
            ret = CL_SUCCESS;
        }
    }
    else
    {
        ret = clGetPlatformInfo(cl_get_handle(platform), param_name, param_value_size, param_value, &size_ret);
        size_t_return_indirect(size_ret, param_value_size_ret);
    }

    TRACE("(%p, 0x%x, %ld, %p, %p)=%d\n", platform, param_name, param_value_size, param_value, param_value_size_ret, ret);
    return ret;
}


/*---------------------------------------------------------------*/
/* Device APIs */

cl_int WINAPI wine_clGetDeviceIDs(wine_cl_platform_id platform, cl_device_type device_type,
                                  cl_uint num_entries, wine_cl_device_id * devices, cl_uint * num_devices)
{
    void *HOSTPTR *ids;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("(%p, 0x%lx, %d, %p, %p)\n", platform, (long unsigned int)device_type, num_entries, devices, num_devices);
    if ((ids = alloc_temp_output_handles(devices, num_entries)))
        ret = convert_output_handles(clGetDeviceIDs(cl_get_handle(platform), device_type, num_entries, (cl_device_id *)ids, num_devices),
                                     devices, ids, num_devices);
    TRACE("(%p, 0x%lx, %d, %p, %p)=%d\n", platform, (long unsigned int)device_type, num_entries, devices, num_devices, ret);
    return ret;
}

cl_int WINAPI wine_clGetDeviceInfo(wine_cl_device_id device, cl_device_info param_name,
                                   SIZE_T param_value_size, void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("(%p, 0x%x, %ld, %p, %p)\n",device, param_name, param_value_size, param_value, param_value_size_ret);

    /* Hide all extensions.
     * TODO: Add individual extension support as needed.
     */
    if (param_name == CL_DEVICE_EXTENSIONS)
    {
        ret = CL_INVALID_VALUE;

        if (param_value && param_value_size > 0)
        {
            char *exts = (char *) param_value;
            exts[0] = '\0';
            ret = CL_SUCCESS;
        }

        if (param_value_size_ret)
        {
            *param_value_size_ret = 1;
            ret = CL_SUCCESS;
        }
    }
    else
    {
        ret = clGetDeviceInfo(cl_get_handle(device), param_name, param_value_size, param_value, &size_ret);
        size_t_return_indirect(size_ret, param_value_size_ret);
    }

    /* Filter out the CL_EXEC_NATIVE_KERNEL flag */
    if (param_name == CL_DEVICE_EXECUTION_CAPABILITIES)
    {
        cl_device_exec_capabilities *caps = (cl_device_exec_capabilities *) param_value;
        *caps &= ~CL_EXEC_NATIVE_KERNEL;
    }

    TRACE("(%p, 0x%x, %ld, %p, %p)=%d\n",device, param_name, param_value_size, param_value, param_value_size_ret, ret);
    return ret;
}


/*---------------------------------------------------------------*/
/* Context APIs  */

typedef struct
{
    void WINAPI (*pfn_notify)(const char *errinfo, const void *private_info, SIZE_T cb, void *user_data);
    void *user_data;
} CONTEXT_CALLBACK;

static void context_fn_notify(const char *HOSTPTR errinfo, const void *HOSTPTR private_info, size_t cb, void *HOSTPTR user_data)
{
    char *err;
    void *private;
    CONTEXT_CALLBACK *ccb;
    TRACE("(%s, %p, %ld, %p)\n", errinfo, private_info, (SIZE_T)cb, user_data);
    err = mirror_callback_string(errinfo);
    private = mirror_callback_data(private_info, cb);
    ccb = ADDRSPACECAST(CONTEXT_CALLBACK *, user_data);
    if(ccb->pfn_notify && (!err == !errinfo) && (!private == !private_info))
        ccb->pfn_notify(err, private, (SIZE_T)cb, ccb->user_data);
    cl_temp_free(err);
    cl_temp_free(private);
    TRACE("Callback COMPLETED\n");
}

wine_cl_context WINAPI wine_clCreateContext(const wine_cl_context_properties * properties, cl_uint num_devices, const wine_cl_device_id * devices,
                                       void WINAPI (*pfn_notify)(const char *errinfo, const void *private_info, SIZE_T cb, void *user_data),
                                       void * user_data, cl_int * errcode_ret)
{
    cl_device_id *devs;
    cl_context ret = NULL;
    CONTEXT_CALLBACK *ccb;
    cl_context_properties props = *properties;

    TRACE("(%p, %d, %p, %p, %p, %p)\n", properties, num_devices, devices, pfn_notify, user_data, errcode_ret);
    /* FIXME: The CONTEXT_CALLBACK structure is currently leaked.
     * Pointers to callback redirectors should be remembered and free()d when the context is destroyed.
     * The problem is determining when a context is being destroyed. clReleaseContext only decrements
     * the use count for a context, its destruction can come much later and therefore there is a risk
     * that the callback could be invoked after the user_data memory has been free()d.
     */
    ccb = heap_alloc(sizeof(CONTEXT_CALLBACK));
    devs = mirror_device_array(devices, num_devices);
    if (ccb && devs)
    {
        ccb->pfn_notify = pfn_notify;
        ccb->user_data = user_data;
        ret = clCreateContext(&props, num_devices, devs, context_fn_notify, ccb, errcode_ret);
    }
    cl_temp_free(devs);
    TRACE("(%p, %d, %p, %p, %p, %p (%d)))=%p\n", properties, num_devices, devices, &pfn_notify, user_data, errcode_ret, errcode_ret ? *errcode_ret : 0, ret);
    return cl_wrap_handle(ret);
}

wine_cl_context WINAPI wine_clCreateContextFromType(const wine_cl_context_properties * properties, cl_device_type device_type,
                                               void WINAPI (*pfn_notify)(const char *errinfo, const void *private_info, SIZE_T cb, void *user_data),
                                               void * user_data, cl_int * errcode_ret)
{
    cl_context ret;
    CONTEXT_CALLBACK *ccb;
    cl_context_properties props = *properties;

    TRACE("(%p, 0x%lx, %p, %p, %p)\n", properties, (long unsigned int)device_type, pfn_notify, user_data, errcode_ret);
    /* FIXME: The CONTEXT_CALLBACK structure is currently leaked.
     * Pointers to callback redirectors should be remembered and free()d when the context is destroyed.
     * The problem is determining when a context is being destroyed. clReleaseContext only decrements
     * the use count for a context, its destruction can come much later and therefore there is a risk
     * that the callback could be invoked after the user_data memory has been free()d.
     */
    ccb = heap_alloc(sizeof(CONTEXT_CALLBACK));
    ccb->pfn_notify = pfn_notify;
    ccb->user_data = user_data;
    ret = clCreateContextFromType(&props, device_type, context_fn_notify, ccb, errcode_ret);
    TRACE("(%p, 0x%lx, %p, %p, %p (%d)))=%p\n", properties, (long unsigned int)device_type, pfn_notify, user_data, errcode_ret, errcode_ret ? *errcode_ret : 0, ret);
    return cl_wrap_handle(ret);
}

cl_int WINAPI wine_clRetainContext(wine_cl_context context)
{
    cl_int ret;
    TRACE("(%p)\n", context);
    cl_retain_handle(context);
    ret = clRetainContext(cl_get_handle(context));
    TRACE("(%p)=%d\n", context, ret);
    return ret;
}

cl_int WINAPI wine_clReleaseContext(wine_cl_context context)
{
    cl_int ret;
    TRACE("(%p)\n", context);
    ret = clReleaseContext(cl_get_handle(context));
    cl_release_handle(context);
    TRACE("(%p)=%d\n", context, ret);
    return ret;
}

cl_int WINAPI wine_clGetContextInfo(wine_cl_context context, cl_context_info param_name,
                                    SIZE_T param_value_size, void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("(%p, 0x%x, %ld, %p, %p)\n", context, param_name, param_value_size, param_value, param_value_size_ret);
    ret = clGetContextInfo(cl_get_handle(context), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    TRACE("(%p, 0x%x, %ld, %p, %p)=%d\n", context, param_name, param_value_size, param_value, param_value_size_ret, ret);
    return ret;
}


/*---------------------------------------------------------------*/
/* Command Queue APIs */

wine_cl_command_queue WINAPI wine_clCreateCommandQueue(wine_cl_context context, wine_cl_device_id device,
                                                  cl_command_queue_properties properties, cl_int * errcode_ret)
{
    cl_command_queue ret;
    TRACE("(%p, %p, 0x%lx, %p)\n", context, device, (long unsigned int)properties, errcode_ret);
    ret = clCreateCommandQueue(cl_get_handle(context), cl_get_handle(device), properties, errcode_ret);
    TRACE("(%p, %p, 0x%lx, %p)=%p\n", context, device, (long unsigned int)properties, errcode_ret, ret);
    return cl_wrap_handle(ret);
}

cl_int WINAPI wine_clRetainCommandQueue(wine_cl_command_queue command_queue)
{
    cl_int ret;
    TRACE("(%p)\n", command_queue);
    cl_retain_handle(command_queue);
    ret = clRetainCommandQueue(cl_get_handle(command_queue));
    TRACE("(%p)=%d\n", command_queue, ret);
    return ret;
}

cl_int WINAPI wine_clReleaseCommandQueue(wine_cl_command_queue command_queue)
{
    cl_int ret;
    TRACE("(%p)\n", command_queue);
    ret = clReleaseCommandQueue(cl_get_handle(command_queue));
    cl_release_handle(command_queue);
    TRACE("(%p)=%d\n", command_queue, ret);
    return ret;
}

cl_int WINAPI wine_clGetCommandQueueInfo(wine_cl_command_queue command_queue, cl_command_queue_info param_name,
                                         SIZE_T param_value_size, void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("%p, %d, %ld, %p, %p\n", command_queue, param_name, param_value_size, param_value, param_value_size_ret);
    ret = clGetCommandQueueInfo(cl_get_handle(command_queue), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}

cl_int WINAPI wine_clSetCommandQueueProperty(wine_cl_command_queue command_queue, cl_command_queue_properties properties, cl_bool enable,
                                             cl_command_queue_properties * old_properties)
{
    FIXME("(%p, 0x%lx, %d, %p): deprecated\n", command_queue, (long unsigned int)properties, enable, old_properties);
    return CL_INVALID_QUEUE_PROPERTIES;
}


/*---------------------------------------------------------------*/
/* Memory Object APIs  */

wine_cl_mem WINAPI wine_clCreateBuffer(wine_cl_context context, cl_mem_flags flags, SIZE_T size, void * host_ptr, cl_int * errcode_ret)
{
    cl_mem ret;
    TRACE("\n");
    ret = clCreateBuffer(cl_get_handle(context), flags, size, host_ptr, errcode_ret);
    return cl_wrap_handle(ret);
}

wine_cl_mem WINAPI wine_clCreateImage2D(wine_cl_context context, cl_mem_flags flags, cl_image_format * image_format,
                                   SIZE_T image_width, SIZE_T image_height, SIZE_T image_row_pitch, void * host_ptr, cl_int * errcode_ret)
{
    cl_mem ret;
    TRACE("\n");
    ret = clCreateImage2D(cl_get_handle(context), flags, image_format, image_width, image_height, image_row_pitch, host_ptr, errcode_ret);
    return cl_wrap_handle(ret);
}

wine_cl_mem WINAPI wine_clCreateImage3D(wine_cl_context context, cl_mem_flags flags, cl_image_format * image_format,
                                   SIZE_T image_width, SIZE_T image_height, SIZE_T image_depth, SIZE_T image_row_pitch, SIZE_T image_slice_pitch,
                                   void * host_ptr, cl_int * errcode_ret)
{
    cl_mem ret;
    TRACE("\n");
    ret = clCreateImage3D(cl_get_handle(context), flags, image_format, image_width, image_height, image_depth, image_row_pitch, image_slice_pitch, host_ptr, errcode_ret);
    return cl_wrap_handle(ret);
}

cl_int WINAPI wine_clRetainMemObject(wine_cl_mem memobj)
{
    cl_int ret;
    TRACE("(%p)\n", memobj);
    cl_retain_handle(memobj);
    ret = clRetainMemObject(cl_get_handle(memobj));
    TRACE("(%p)=%d\n", memobj, ret);
    return ret;
}

cl_int WINAPI wine_clReleaseMemObject(wine_cl_mem memobj)
{
    cl_int ret;
    TRACE("(%p)\n", memobj);
    ret = clReleaseMemObject(cl_get_handle(memobj));
    cl_release_handle(memobj);
    TRACE("(%p)=%d\n", memobj, ret);
    return ret;
}

cl_int WINAPI wine_clGetSupportedImageFormats(wine_cl_context context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries,
                                              cl_image_format * image_formats, cl_uint * num_image_formats)
{
    cl_int ret;
    TRACE("\n");
    ret = clGetSupportedImageFormats(cl_get_handle(context), flags, image_type, num_entries, image_formats, num_image_formats);
    return ret;
}

cl_int WINAPI wine_clGetMemObjectInfo(wine_cl_mem memobj, cl_mem_info param_name, SIZE_T param_value_size, void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("\n");
    ret = clGetMemObjectInfo(cl_get_handle(memobj), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}

cl_int WINAPI wine_clGetImageInfo(wine_cl_mem image, cl_image_info param_name, SIZE_T param_value_size, void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("\n");
    ret = clGetImageInfo(cl_get_handle(image), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}


/*---------------------------------------------------------------*/
/* Sampler APIs  */

wine_cl_sampler WINAPI wine_clCreateSampler(wine_cl_context context, cl_bool normalized_coords, cl_addressing_mode addressing_mode,
                                       cl_filter_mode filter_mode, cl_int * errcode_ret)
{
    TRACE("\n");
    return cl_wrap_handle(clCreateSampler(cl_get_handle(context), normalized_coords, addressing_mode, filter_mode, errcode_ret));
}

cl_int WINAPI wine_clRetainSampler(wine_cl_sampler sampler)
{
    cl_int ret;
    TRACE("\n");
    cl_retain_handle(sampler);
    ret = clRetainSampler(cl_get_handle(sampler));
    return ret;
}

cl_int WINAPI wine_clReleaseSampler(wine_cl_sampler sampler)
{
    cl_int ret;
    TRACE("\n");
    ret = clReleaseSampler(cl_get_handle(sampler));
    cl_release_handle(sampler);
    return ret;
}

cl_int WINAPI wine_clGetSamplerInfo(wine_cl_sampler sampler, cl_sampler_info param_name, SIZE_T param_value_size,
                                    void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("\n");
    ret = clGetSamplerInfo(cl_get_handle(sampler), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}


/*---------------------------------------------------------------*/
/* Program Object APIs  */

wine_cl_program WINAPI wine_clCreateProgramWithSource(wine_cl_context context, cl_uint count, const char ** strings,
                                                 const SIZE_T * lengths, cl_int * errcode_ret)
{
    size_t *lens = NULL;
    const char *HOSTPTR *sources;
    cl_program ret = NULL;
    TRACE("\n");
    if ((sources = mirror_string_array(strings, count)) && (lens = mirror_size_t_array(lengths, count)))
        ret = clCreateProgramWithSource(cl_get_handle(context), count, sources, lens, errcode_ret);
    cl_temp_free(sources);
    cl_temp_free(lens);
    return cl_wrap_handle(ret);
}

wine_cl_program WINAPI wine_clCreateProgramWithBinary(wine_cl_context context, cl_uint num_devices, const wine_cl_device_id * device_list,
                                                 const SIZE_T * lengths, const unsigned char ** binaries, cl_int * binary_status,
                                                 cl_int * errcode_ret)
{
    const unsigned char *HOSTPTR *bins = NULL;
    cl_program ret = NULL;
    size_t *lens = NULL;
    cl_device_id *devs;

    TRACE("\n");
    if ((devs = mirror_device_array(device_list, num_devices)) && (bins = mirror_string_array((const char **)binaries, num_devices))
            && (lens = mirror_size_t_array(lengths, num_devices)))
        ret = clCreateProgramWithBinary(cl_get_handle(context), num_devices, devs, lens, bins, binary_status, errcode_ret);
    cl_temp_free(devs);
    cl_temp_free(bins);
    cl_temp_free(lens);
    return cl_wrap_handle(ret);
}

cl_int WINAPI wine_clRetainProgram(wine_cl_program program)
{
    cl_int ret;
    TRACE("\n");
    cl_retain_handle(program);
    ret = clRetainProgram(cl_get_handle(program));
    return ret;
}

cl_int WINAPI wine_clReleaseProgram(wine_cl_program program)
{
    cl_int ret;
    TRACE("\n");
    ret = clReleaseProgram(cl_get_handle(program));
    cl_release_handle(program);
    return ret;
}

typedef struct
{
    void WINAPI (*pfn_notify)(wine_cl_program program, void * user_data);
    void *user_data;
    wine_cl_program program;
    cl_device_id *devs;
} PROGRAM_CALLBACK;

static void program_fn_notify(cl_program program, void *HOSTPTR user_data)
{
    PROGRAM_CALLBACK *pcb;
    TRACE("(%p, %p)\n", program, user_data);
    pcb = ADDRSPACECAST(PROGRAM_CALLBACK *, user_data);
    pcb->pfn_notify(pcb->program, pcb->user_data);
    cl_temp_free(pcb->devs);
    heap_free(pcb);
    TRACE("Callback COMPLETED\n");
}

cl_int WINAPI wine_clBuildProgram(wine_cl_program program, cl_uint num_devices, const wine_cl_device_id * device_list, const char * options,
                                  void WINAPI (*pfn_notify)(wine_cl_program program, void * user_data),
                                  void * user_data)
{
    cl_device_id *devs;
    cl_int ret;

    TRACE("\n");

    if (!(devs = mirror_device_array(device_list, num_devices)))
        return CL_OUT_OF_HOST_MEMORY;

    if(pfn_notify)
    {
        /* When pfn_notify is provided, clBuildProgram is asynchronous */
        PROGRAM_CALLBACK *pcb;
        pcb = heap_alloc(sizeof(PROGRAM_CALLBACK));
        pcb->pfn_notify = pfn_notify;
        pcb->user_data = user_data;
        pcb->program = program;
        pcb->devs = devs;
        ret = clBuildProgram(cl_get_handle(program), num_devices, devs, options, program_fn_notify, pcb);
    }
    else
    {
        /* When pfn_notify is NULL, clBuildProgram is synchronous */
        ret = clBuildProgram(cl_get_handle(program), num_devices, devs, options, NULL, user_data);
        cl_temp_free(devs);
    }
    return ret;
}

cl_int WINAPI wine_clUnloadCompiler(void)
{
    cl_int ret;
    TRACE("()\n");
    ret = clUnloadCompiler();
    TRACE("()=%d\n", ret);
    return ret;
}

cl_int WINAPI wine_clGetProgramInfo(wine_cl_program program, cl_program_info param_name,
                                    SIZE_T param_value_size, void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("\n");
    ret = clGetProgramInfo(cl_get_handle(program), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}

cl_int WINAPI wine_clGetProgramBuildInfo(wine_cl_program program, wine_cl_device_id device,
                                         cl_program_build_info param_name, SIZE_T param_value_size, void * param_value,
                                         SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("\n");
    ret = clGetProgramBuildInfo(cl_get_handle(program), cl_get_handle(device), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}


/*---------------------------------------------------------------*/
/* Kernel Object APIs */

wine_cl_kernel WINAPI wine_clCreateKernel(wine_cl_program program, char * kernel_name, cl_int * errcode_ret)
{
    cl_kernel ret;
    TRACE("\n");
    ret = clCreateKernel(cl_get_handle(program), kernel_name, errcode_ret);
    return cl_wrap_handle(ret);
}

cl_int WINAPI wine_clCreateKernelsInProgram(wine_cl_program program, cl_uint num_kernels,
                                            wine_cl_kernel * kernels, cl_uint * num_kernels_ret)
{
    void *HOSTPTR *kerns;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    if ((kerns = alloc_temp_output_handles(kernels, num_kernels)))
        ret = convert_output_handles(clCreateKernelsInProgram(cl_get_handle(program), num_kernels, (cl_kernel *)kerns, num_kernels_ret), kernels, kerns, num_kernels_ret);
    return ret;
}

cl_int WINAPI wine_clRetainKernel(wine_cl_kernel kernel)
{
    cl_int ret;
    TRACE("\n");
    cl_retain_handle(kernel);
    ret = clRetainKernel(cl_get_handle(kernel));
    return ret;
}

cl_int WINAPI wine_clReleaseKernel(wine_cl_kernel kernel)
{
    cl_int ret;
    TRACE("\n");
    ret = clReleaseKernel(cl_get_handle(kernel));
    cl_release_handle(kernel);
    return ret;
}

cl_int WINAPI wine_clSetKernelArg(wine_cl_kernel kernel, cl_uint arg_index, SIZE_T arg_size, void * arg_value)
{
    cl_int ret;
    TRACE("\n");
    ret = clSetKernelArg(cl_get_handle(kernel), arg_index, arg_size, arg_value);
    return ret;
}

cl_int WINAPI wine_clGetKernelInfo(wine_cl_kernel kernel, cl_kernel_info param_name,
                                   SIZE_T param_value_size, void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("\n");
    ret = clGetKernelInfo(cl_get_handle(kernel), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}

cl_int WINAPI wine_clGetKernelWorkGroupInfo(wine_cl_kernel kernel, wine_cl_device_id device,
                                            cl_kernel_work_group_info param_name, SIZE_T param_value_size,
                                            void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("\n");
    ret = clGetKernelWorkGroupInfo(cl_get_handle(kernel), cl_get_handle(device), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}


/*---------------------------------------------------------------*/
/* Event Object APIs  */

cl_int WINAPI wine_clWaitForEvents(cl_uint num_events, wine_cl_event * event_list)
{
#ifdef __i386_on_x86_64__
    /* clWaitForEvents() should never fail on valid input, so use a local array. */
    cl_event events[WINE_CL_LOCAL_EVENTS];
    cl_uint i, end, src = 0;
    cl_int ret = CL_SUCCESS;
    TRACE("\n");
    do
    {
        end = min(WINE_CL_LOCAL_EVENTS, num_events - src);
        for (i = 0; i < end; ++i)
            events[i] = cl_get_handle(event_list[src + i]);
        src += i;
        ret = clWaitForEvents(i, events);
    } while (src < num_events && ret == CL_SUCCESS);
#else
    cl_int ret;
    TRACE("\n");
    ret = clWaitForEvents(num_events, event_list);
#endif
    return ret;
}

cl_int WINAPI wine_clGetEventInfo(wine_cl_event event, cl_event_info param_name, SIZE_T param_value_size,
                                  void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("\n");
    ret = clGetEventInfo(cl_get_handle(event), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}

cl_int WINAPI wine_clRetainEvent(wine_cl_event event)
{
    cl_int ret;
    TRACE("\n");
    cl_retain_handle(event);
    ret = clRetainEvent(cl_get_handle(event));
    return ret;
}

cl_int WINAPI wine_clReleaseEvent(wine_cl_event event)
{
    cl_int ret;
    TRACE("\n");
    ret = clReleaseEvent(cl_get_handle(event));
    cl_release_handle(event);
    return ret;
}


/*---------------------------------------------------------------*/
/* Profiling APIs  */

cl_int WINAPI wine_clGetEventProfilingInfo(wine_cl_event event, cl_profiling_info param_name, SIZE_T param_value_size,
                                           void * param_value, SIZE_T * param_value_size_ret)
{
    size_t size_ret;
    cl_int ret;
    TRACE("\n");
    ret = clGetEventProfilingInfo(cl_get_handle(event), param_name, param_value_size, param_value, &size_ret);
    size_t_return_indirect(size_ret, param_value_size_ret);
    return ret;
}


/*---------------------------------------------------------------*/
/* Flush and Finish APIs */

cl_int WINAPI wine_clFlush(wine_cl_command_queue command_queue)
{
    cl_int ret;
    TRACE("(%p)\n", command_queue);
    ret = clFlush(cl_get_handle(command_queue));
    TRACE("(%p)=%d\n", command_queue, ret);
    return ret;
}

cl_int WINAPI wine_clFinish(wine_cl_command_queue command_queue)
{
    cl_int ret;
    TRACE("(%p)\n", command_queue);
    ret = clFinish(cl_get_handle(command_queue));
    TRACE("(%p)=%d\n", command_queue, ret);
    return ret;
}


/*---------------------------------------------------------------*/
/* Enqueued Commands APIs */

cl_int WINAPI wine_clEnqueueReadBuffer(wine_cl_command_queue command_queue, wine_cl_mem buffer, cl_bool blocking_read,
                                       SIZE_T offset, SIZE_T cb, void * ptr,
                                       cl_uint num_events_in_wait_list, const wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueReadBuffer(cl_get_handle(command_queue), cl_get_handle(buffer), blocking_read, offset, cb,
                                  ptr, num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueWriteBuffer(wine_cl_command_queue command_queue, wine_cl_mem buffer, cl_bool blocking_write,
                                        SIZE_T offset, SIZE_T cb, const void * ptr,
                                        cl_uint num_events_in_wait_list, const wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueWriteBuffer(cl_get_handle(command_queue), cl_get_handle(buffer), blocking_write, offset, cb,
                                   ptr, num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueCopyBuffer(wine_cl_command_queue command_queue, wine_cl_mem src_buffer, wine_cl_mem dst_buffer,
                                       SIZE_T src_offset, SIZE_T dst_offset, SIZE_T cb,
                                       cl_uint num_events_in_wait_list, const wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueCopyBuffer(cl_get_handle(command_queue), cl_get_handle(src_buffer), cl_get_handle(dst_buffer), src_offset, dst_offset, cb,
                                  num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueReadImage(wine_cl_command_queue command_queue, wine_cl_mem image, cl_bool blocking_read,
                                      const SIZE_T * origin, const SIZE_T * region,
                                      SIZE_T row_pitch, SIZE_T slice_pitch, void * ptr,
                                      cl_uint num_events_in_wait_list, const wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    size_t origin_h[3], region_h[3];
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("(%p, %p, %d, %p, %p, %ld, %ld, %p, %d, %p, %p)\n", command_queue, image, blocking_read,
          origin, region, row_pitch, slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
    convert_size_t_array(origin_h, origin, 3);
    convert_size_t_array(region_h, region, 3);
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueReadImage(cl_get_handle(command_queue), cl_get_handle(image), blocking_read, origin_h, region_h, row_pitch, slice_pitch,
                                 ptr, num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    TRACE("(%p, %p, %d, %p, %p, %ld, %ld, %p, %d, %p, %p)=%d\n", command_queue, image, blocking_read,
          origin, region, row_pitch, slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event, ret);
    return ret;
}

cl_int WINAPI wine_clEnqueueWriteImage(wine_cl_command_queue command_queue, wine_cl_mem image, cl_bool blocking_write,
                                       const SIZE_T * origin, const SIZE_T * region,
                                       SIZE_T input_row_pitch, SIZE_T input_slice_pitch, const void * ptr,
                                       cl_uint num_events_in_wait_list, const wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    size_t origin_h[3], region_h[3];
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    convert_size_t_array(origin_h, origin, 3);
    convert_size_t_array(region_h, region, 3);
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueWriteImage(cl_get_handle(command_queue), cl_get_handle(image), blocking_write, origin_h, region_h, input_row_pitch, input_slice_pitch,
                                  ptr, num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueCopyImage(wine_cl_command_queue command_queue, wine_cl_mem src_image, wine_cl_mem dst_image,
                                      SIZE_T * src_origin, SIZE_T * dst_origin, SIZE_T * region,
                                      cl_uint num_events_in_wait_list, wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    size_t src_origin_h[3], dst_origin_h[3], region_h[3];
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    convert_size_t_array(src_origin_h, src_origin, 3);
    convert_size_t_array(dst_origin_h, dst_origin, 3);
    convert_size_t_array(region_h, region, 3);
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueCopyImage(cl_get_handle(command_queue), cl_get_handle(src_image), cl_get_handle(dst_image), src_origin_h, dst_origin_h, region_h,
                                 num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueCopyImageToBuffer(wine_cl_command_queue command_queue, wine_cl_mem src_image, wine_cl_mem dst_buffer,
                                              SIZE_T * src_origin, SIZE_T * region, SIZE_T dst_offset,
                                              cl_uint num_events_in_wait_list, wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    size_t src_origin_h[3], region_h[3];
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    convert_size_t_array(src_origin_h, src_origin, 3);
    convert_size_t_array(region_h, region, 3);
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueCopyImageToBuffer(cl_get_handle(command_queue), cl_get_handle(src_image), cl_get_handle(dst_buffer), src_origin_h, region_h,
                                         dst_offset, num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueCopyBufferToImage(wine_cl_command_queue command_queue, wine_cl_mem src_buffer, wine_cl_mem dst_image,
                                              SIZE_T src_offset, SIZE_T * dst_origin, SIZE_T * region,
                                              cl_uint num_events_in_wait_list, wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    size_t dst_origin_h[3], region_h[3];
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    convert_size_t_array(dst_origin_h, dst_origin, 3);
    convert_size_t_array(region_h, region, 3);
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueCopyBufferToImage(cl_get_handle(command_queue), cl_get_handle(src_buffer), cl_get_handle(dst_image), src_offset, dst_origin_h, region_h,
                                         num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ret;
}

void * WINAPI wine_clEnqueueMapBuffer(wine_cl_command_queue command_queue, wine_cl_mem buffer, cl_bool blocking_map,
                                      cl_map_flags map_flags, SIZE_T offset, SIZE_T cb,
                                      cl_uint num_events_in_wait_list, wine_cl_event * event_wait_list, wine_cl_event * event, cl_int * errcode_ret)
{
    cl_event *events;
    cl_event event_ret = NULL;
    void *HOSTPTR ret = NULL;
    TRACE("\n");
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueMapBuffer(cl_get_handle(command_queue), cl_get_handle(buffer), blocking_map, map_flags, offset, cb,
                                 num_events_in_wait_list, events, event ? &event_ret : NULL, errcode_ret);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ADDRSPACECAST(void *, ret);
}

void * WINAPI wine_clEnqueueMapImage(wine_cl_command_queue command_queue, wine_cl_mem image, cl_bool blocking_map,
                                     cl_map_flags map_flags, SIZE_T * origin, SIZE_T * region,
                                     SIZE_T * image_row_pitch, SIZE_T * image_slice_pitch,
                                     cl_uint num_events_in_wait_list, wine_cl_event * event_wait_list, wine_cl_event * event, cl_int * errcode_ret)
{
    cl_event *events;
    cl_event event_ret = NULL;
    size_t origin_h[3], region_h[3];
    size_t row_pitch = 0, slice_pitch = 0;
    void *HOSTPTR ret = NULL;

    TRACE("\n");
    convert_size_t_array(origin_h, origin, 3);
    convert_size_t_array(region_h, region, 3);
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueMapImage(cl_get_handle(command_queue), cl_get_handle(image), blocking_map, map_flags, origin_h, region_h, &row_pitch, &slice_pitch,
                                num_events_in_wait_list, events, event ? &event_ret : NULL, errcode_ret);
    size_t_return_indirect(row_pitch, image_row_pitch);
    size_t_return_indirect(slice_pitch, image_slice_pitch);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ADDRSPACECAST(void *, ret);
}

cl_int WINAPI wine_clEnqueueUnmapMemObject(wine_cl_command_queue command_queue, wine_cl_mem memobj, void * mapped_ptr,
                                           cl_uint num_events_in_wait_list, wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueUnmapMemObject(cl_get_handle(command_queue), cl_get_handle(memobj), mapped_ptr,
                                      num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueNDRangeKernel(wine_cl_command_queue command_queue, wine_cl_kernel kernel, cl_uint work_dim,
                                          SIZE_T * global_work_offset, SIZE_T * global_work_size, SIZE_T * local_work_size,
                                          cl_uint num_events_in_wait_list, wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    size_t *global_sizes;
    size_t *local_sizes;
    cl_event event_ret = NULL;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    (void)global_work_offset; /* Currently must be NULL, but this may change in a future openCL version. */
    global_sizes = mirror_size_t_array(global_work_size, work_dim);
    local_sizes = mirror_size_t_array(local_work_size, work_dim); /* Can be NULL */
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (global_sizes && mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueNDRangeKernel(cl_get_handle(command_queue), cl_get_handle(kernel), work_dim, NULL, global_sizes, local_sizes,
                                     num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(global_sizes);
    cl_temp_free(local_sizes);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueTask(wine_cl_command_queue command_queue, wine_cl_kernel kernel,
                                 cl_uint num_events_in_wait_list, wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_event *events;
    cl_event event_ret = NULL;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    events = mirror_event_array(event_wait_list, num_events_in_wait_list);
    if (mirroring_succeeded(event_wait_list, events))
        ret = clEnqueueTask(cl_get_handle(command_queue), cl_get_handle(kernel),
                            num_events_in_wait_list, events, event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueNativeKernel(wine_cl_command_queue command_queue,
                                         void WINAPI (*user_func)(void *args),
                                         void * args, SIZE_T cb_args,
                                         cl_uint num_mem_objects, const wine_cl_mem * mem_list, const void ** args_mem_loc,
                                         cl_uint num_events_in_wait_list, const wine_cl_event * event_wait_list, wine_cl_event * event)
{
    cl_int ret = CL_INVALID_OPERATION;
    /* FIXME: There appears to be no obvious method for translating the ABI for user_func.
     * There is no opaque user_data structure passed, that could encapsulate the return address.
     * The OpenCL specification seems to indicate that args has an implementation specific
     * structure that cannot be used to stash away a return address for the WINAPI user_func.
     */
#if 0
    ret = clEnqueueNativeKernel(command_queue, user_func, args, cb_args, num_mem_objects, mem_list, args_mem_loc,
                                 num_events_in_wait_list, event_wait_list, event);
#else
    FIXME("not supported due to user_func ABI mismatch\n");
#endif
    return ret;
}

cl_int WINAPI wine_clEnqueueMarker(wine_cl_command_queue command_queue, wine_cl_event * event)
{
    cl_event event_ret = NULL;
    cl_int ret;
    TRACE("\n");
    ret = clEnqueueMarker(cl_get_handle(command_queue), event ? &event_ret : NULL);
    cl_wrap_handle_indirect(event_ret, event);
    return ret;
}

cl_int WINAPI wine_clEnqueueWaitForEvents(wine_cl_command_queue command_queue, cl_uint num_events, wine_cl_event * event_list)
{
    cl_event *events;
    cl_int ret = CL_OUT_OF_HOST_MEMORY;
    TRACE("\n");
    events = mirror_event_array(event_list, num_events);
    if (mirroring_succeeded(event_list, events))
        ret = clEnqueueWaitForEvents(cl_get_handle(command_queue), num_events, events);
    cl_temp_free(events);
    return ret;
}

cl_int WINAPI wine_clEnqueueBarrier(wine_cl_command_queue command_queue)
{
    cl_int ret;
    TRACE("\n");
    ret = clEnqueueBarrier(cl_get_handle(command_queue));
    return ret;
}


/*---------------------------------------------------------------*/
/* Extension function access */

void * WINAPI wine_clGetExtensionFunctionAddress(const char * func_name)
{
    void * ret = 0;
    TRACE("(%s)\n",func_name);
#if 0
    ret = clGetExtensionFunctionAddress(func_name);
#else
    FIXME("extensions not implemented\n");
#endif
    TRACE("(%s)=%p\n",func_name, ret);
    return ret;
}


#if OPENCL_WITH_GL
/*---------------------------------------------------------------*/
/* Khronos-approved (KHR) OpenCL extensions which have OpenGL dependencies. */

wine_cl_mem WINAPI wine_clCreateFromGLBuffer(wine_cl_context context, cl_mem_flags flags, cl_GLuint bufobj, int * errcode_ret)
{
}

wine_cl_mem WINAPI wine_clCreateFromGLTexture2D(wine_cl_context context, cl_mem_flags flags, cl_GLenum target,
                                           cl_GLint miplevel, cl_GLuint texture, cl_int * errcode_ret)
{
}

wine_cl_mem WINAPI wine_clCreateFromGLTexture3D(wine_cl_context context, cl_mem_flags flags, cl_GLenum target,
                                           cl_GLint miplevel, cl_GLuint texture, cl_int * errcode_ret)
{
}

wine_cl_mem WINAPI wine_clCreateFromGLRenderbuffer(wine_cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int * errcode_ret)
{
}

cl_int WINAPI wine_clGetGLObjectInfo(wine_cl_mem memobj, cl_gl_object_type * gl_object_type, cl_GLuint * gl_object_name)
{
}

cl_int WINAPI wine_clGetGLTextureInfo(wine_cl_mem memobj, cl_gl_texture_info param_name, SIZE_T param_value_size,
                                      void * param_value, SIZE_T * param_value_size_ret)
{
}

cl_int WINAPI wine_clEnqueueAcquireGLObjects(wine_cl_command_queue command_queue, cl_uint num_objects, const wine_cl_mem * mem_objects,
                                             cl_uint num_events_in_wait_list, const wine_cl_event * event_wait_list, wine_cl_event * event)
{
}

cl_int WINAPI wine_clEnqueueReleaseGLObjects(wine_cl_command_queue command_queue, cl_uint num_objects, const wine_cl_mem * mem_objects,
                                             cl_uint num_events_in_wait_list, const wine_cl_event * event_wait_list, wine_cl_event * event)
{
}


/*---------------------------------------------------------------*/
/* cl_khr_gl_sharing extension  */

cl_int WINAPI wine_clGetGLContextInfoKHR(const wine_cl_context_properties * properties, cl_gl_context_info param_name,
                                         SIZE_T param_value_size, void * param_value, SIZE_T * param_value_size_ret)
{
}

#endif


#if 0
/*---------------------------------------------------------------*/
/* cl_khr_icd extension */

cl_int WINAPI wine_clIcdGetPlatformIDsKHR(cl_uint num_entries, wine_cl_platform_id * platforms, cl_uint * num_platforms)
{
}
#endif

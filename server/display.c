/*
 * Server-side display device management
 *
 * Copyright (C) 2021 Zhiyi Zhang for CodeWeavers
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

#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"

#include "request.h"
#include "user.h"

static struct list monitor_list = LIST_INIT(monitor_list);

/* retrieve a pointer to a monitor from its handle */
static struct monitor *get_monitor( user_handle_t handle )
{
    struct monitor *monitor;

    if (!(monitor = get_user_object( handle, USER_MONITOR )) || monitor->removed)
        set_win32_error( ERROR_INVALID_MONITOR_HANDLE );
    return monitor;
}

/* create a monitor */
static struct monitor *create_monitor( const struct unicode_str *adapter_name,
                                       const rectangle_t *monitor_rect,
                                       const rectangle_t *work_rect,
                                       unsigned int serial_no)
{
    struct monitor *monitor;

    LIST_FOR_EACH_ENTRY( monitor, &monitor_list, struct monitor, entry )
    {
        if (monitor->removed && monitor->serial_no == serial_no &&
                monitor->adapter_name_len == adapter_name->len &&
                !memcmp(monitor->adapter_name, adapter_name->str, adapter_name->len))
        {
            monitor->monitor_rect = *monitor_rect;
            monitor->work_rect = *work_rect;
            monitor->removed = 0;
            return monitor;
        }
    }

    if (!(monitor = mem_alloc( sizeof(*monitor) )))
        goto failed;

    if (!(monitor->adapter_name = memdup( adapter_name->str, adapter_name->len )))
        goto failed;
    monitor->adapter_name_len = adapter_name->len;

    if (!(monitor->handle = alloc_user_handle( monitor, USER_MONITOR )))
        goto failed;

    monitor->monitor_rect = *monitor_rect;
    monitor->work_rect = *work_rect;
    monitor->serial_no = serial_no;
    monitor->removed = 0;
    list_add_tail( &monitor_list, &monitor->entry );
    return monitor;

failed:
    if (monitor)
    {
        if (monitor->adapter_name)
            free( monitor->adapter_name );
        free( monitor );
    }
    return NULL;
}

/* create a monitor */
DECL_HANDLER(create_monitor)
{
    struct unicode_str adapter_name;
    struct monitor *monitor;

    adapter_name = get_req_unicode_str();
    if ((monitor = create_monitor( &adapter_name, &req->monitor_rect,
                    &req->work_rect, req->serial_no )))
        reply->handle = monitor->handle;
}

/* get information about a monitor */
DECL_HANDLER(get_monitor_info)
{
    struct monitor *monitor;

    if (!(monitor = get_monitor( req->handle )))
        return;

    reply->monitor_rect = monitor->monitor_rect;
    reply->work_rect = monitor->work_rect;
    set_reply_data( monitor->adapter_name, min(monitor->adapter_name_len, get_reply_max_size()) );
    return;
}

/* enumerate monitors */
DECL_HANDLER(enum_monitor)
{
    struct monitor *monitor;
    unsigned int index = 0;

    LIST_FOR_EACH_ENTRY( monitor, &monitor_list, struct monitor, entry )
    {
        if (monitor->removed)
            continue;
        if (req->index > index++)
            continue;

        reply->handle = monitor->handle;
        reply->monitor_rect = monitor->monitor_rect;
        return;
    }
    set_error( STATUS_NO_MORE_ENTRIES );
}

/* destroy a monitor */
DECL_HANDLER(destroy_monitor)
{
    struct monitor *monitor;

    if (!(monitor = get_monitor( req->handle )))
        return;
    monitor->removed = 1;
}

/*  Support for the XBOX360 controller on OS/X
 *
 * Copyright 2019 CodeWeavers, Aric Stewart
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

#if defined(HAVE_IOKIT_USB_IOUSBLIB_H)
#define DWORD UInt32
#define LPDWORD UInt32*
#define LONG SInt32
#define LPLONG SInt32*
#define LPVOID __carbon_LPVOID
#define E_PENDING __carbon_E_PENDING
#define ULONG __carbon_ULONG
#define E_INVALIDARG __carbon_E_INVALIDARG
#define E_OUTOFMEMORY __carbon_E_OUTOFMEMORY
#define E_HANDLE __carbon_E_HANDLE
#define E_ACCESSDENIED __carbon_E_ACCESSDENIED
#define E_UNEXPECTED __carbon_E_UNEXPECTED
#define E_FAIL __carbon_E_FAIL
#define E_ABORT __carbon_E_ABORT
#define E_POINTER __carbon_E_POINTER
#define E_NOINTERFACE __carbon_E_NOINTERFACE
#define E_NOTIMPL __carbon_E_NOTIMPL
#define S_FALSE __carbon_S_FALSE
#define S_OK __carbon_S_OK
#define HRESULT_FACILITY __carbon_HRESULT_FACILITY
#define IS_ERROR __carbon_IS_ERROR
#define FAILED __carbon_FAILED
#define SUCCEEDED __carbon_SUCCEEDED
#define MAKE_HRESULT __carbon_MAKE_HRESULT
#define HRESULT __carbon_HRESULT
#define STDMETHODCALLTYPE __carbon_STDMETHODCALLTYPE
#define PAGE_SHIFT __carbon_PAGE_SHIFT
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#undef LPVOID
#undef ULONG
#undef E_INVALIDARG
#undef E_OUTOFMEMORY
#undef E_HANDLE
#undef E_ACCESSDENIED
#undef E_UNEXPECTED
#undef E_FAIL
#undef E_ABORT
#undef E_POINTER
#undef E_NOINTERFACE
#undef E_NOTIMPL
#undef S_FALSE
#undef S_OK
#undef HRESULT_FACILITY
#undef IS_ERROR
#undef FAILED
#undef SUCCEEDED
#undef MAKE_HRESULT
#undef HRESULT
#undef STDMETHODCALLTYPE
#undef DWORD
#undef LPDWORD
#undef LONG
#undef LPLONG
#undef E_PENDING
#undef PAGE_SHIFT
#endif /* HAVE_IOKIT_USB_IOUSBLIB_H */

#define NONAMELESSUNION

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "ddk/hidtypes.h"
#include "wine/debug.h"

#include "bus.h"

WINE_DEFAULT_DEBUG_CHANNEL(plugplay);

#ifdef HAVE_IOSERVICEMATCHING

static DRIVER_OBJECT *xbox_driver_obj = NULL;
static CFRunLoopRef run_loop;
static HANDLE run_loop_handle;

static const WCHAR busidW[] = {'X','B','O','X',0};
#include "initguid.h"
DEFINE_GUID(DEVCLASS, 0x2381EBD6,0x852A,0x464D,0x95,0x2D,0xF8,0x4D,0x08,0x35,0xDE,0xBA);

struct platform_private {
    io_object_t object;
    IOUSBDeviceInterface500 **dev;

    IOUSBInterfaceInterface550 **interface;
    CFRunLoopSourceRef source;

    BOOL started;
    char buffer[64];
};

static const unsigned char ReportDescriptor[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x01,                    //   USAGE_PAGE (Generic Desktop)
    0x09, 0x3a,                    //   USAGE (Counted Buffer)
    0xa1, 0x02,                    //   COLLECTION (Logical)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x3f,                    //     USAGE (Reserved)
    0x09, 0x3b,                    //     USAGE (Byte Count)
    0x81, 0x01,                    //     INPUT (Cnst,Ary,Abs)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
    0x45, 0x01,                    //     PHYSICAL_MAXIMUM (1)
    0x95, 0x04,                    //     REPORT_COUNT (4)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x0c,                    //     USAGE_MINIMUM (Button 12)
    0x29, 0x0f,                    //     USAGE_MAXIMUM (Button 15)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
    0x45, 0x01,                    //     PHYSICAL_MAXIMUM (1)
    0x95, 0x04,                    //     REPORT_COUNT (4)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x09, 0x09,                    //     USAGE (Button 9)
    0x09, 0x0a,                    //     USAGE (Button 10)
    0x09, 0x07,                    //     USAGE (Button 7)
    0x09, 0x08,                    //     USAGE (Button 8)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
    0x45, 0x01,                    //     PHYSICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x09, 0x05,                    //     USAGE (Button 5)
    0x09, 0x06,                    //     USAGE (Button 6)
    0x09, 0x0b,                    //     USAGE (Button 11)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x81, 0x01,                    //     INPUT (Cnst,Ary,Abs)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
    0x45, 0x01,                    //     PHYSICAL_MAXIMUM (1)
    0x95, 0x04,                    //     REPORT_COUNT (4)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x04,                    //     USAGE_MAXIMUM (Button 4)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
    0x46, 0xff, 0x00,              //     PHYSICAL_MAXIMUM (255)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x32,                    //     USAGE (Z)
    0x09, 0x35,                    //     USAGE (Rz)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x75, 0x10,                    //     REPORT_SIZE (16)
    0x16, 0x00, 0x80,              //     LOGICAL_MINIMUM (-32768)
    0x26, 0xff, 0x7f,              //     LOGICAL_MAXIMUM (32767)
    0x36, 0x00, 0x80,              //     PHYSICAL_MINIMUM (-32768)
    0x46, 0xff, 0x7f,              //     PHYSICAL_MAXIMUM (32767)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    //     USAGE (Pointer)
    0xa1, 0x00,                    //     COLLECTION (Physical)
    0x95, 0x02,                    //       REPORT_COUNT (2)
    0x05, 0x01,                    //       USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //       USAGE (X)
    0x09, 0x31,                    //       USAGE (Y)
    0x81, 0x02,                    //       INPUT (Data,Var,Abs)
    0xc0,                          //     END_COLLECTION
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    //     USAGE (Pointer)
    0xa1, 0x00,                    //     COLLECTION (Physical)
    0x95, 0x02,                    //       REPORT_COUNT (2)
    0x05, 0x01,                    //       USAGE_PAGE (Generic Desktop)
    0x09, 0x33,                    //       USAGE (Rx)
    0x09, 0x34,                    //       USAGE (Ry)
    0x81, 0x02,                    //       INPUT (Data,Var,Abs)
    0xc0,                          //     END_COLLECTION
    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION
};

typedef struct _HEADER {
    CHAR cmd;
    CHAR size;
} __attribute__((__packed__)) HEADER;

typedef struct _IN_REPORT {
    HEADER header;
    SHORT buttons;
    CHAR trigger_left;
    CHAR trigger_right;
    SHORT hat_left_x;
    SHORT hat_left_y;
    SHORT hat_right_x;
    SHORT hat_right_y;
    CHAR reserved[6];
} __attribute__((__packed__)) IN_REPORT;

typedef struct _OUT_REPORT {
    HEADER header;
    char data[1];
} __attribute__((__packed__)) OUT_REPORT;

static int xbox_compare_platform_device(DEVICE_OBJECT *device, void * HOSTPTR platform_dev)
{
    struct platform_private *ext = get_platform_private(device);
    io_object_t dev2 = (io_object_t)(size_t)platform_dev;

    return IOObjectIsEqualTo(dev2, ext->object);
}

static HRESULT get_device_string(IOUSBDeviceInterface500 **dev, UInt8 idx, WCHAR *buffer, DWORD length)
{
    UInt16 buf[64];
    IOUSBDevRequest request;
    int count;

    request.bmRequestType = USBmakebmRequestType( kUSBIn, kUSBStandard, kUSBDevice );
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = (kUSBStringDesc << 8) | idx;
    request.wIndex = 0x409;
    request.wLength = sizeof(buf);
    request.pData = buf;

    kern_return_t err = (*dev)->DeviceRequest(dev, &request);
    if ( err != 0 )
        return STATUS_UNSUCCESSFUL;

   count = ( request.wLenDone - 1 ) / 2;

    if ((count+1) > length)
        return STATUS_BUFFER_TOO_SMALL;

    lstrcpynW(buffer, &buf[1], count+1);
    buffer[count+1] = 0;

    return STATUS_SUCCESS;
}

/* Handlers */

static NTSTATUS xbox_get_reportdescriptor(DEVICE_OBJECT *device, BYTE *buffer, DWORD length, DWORD *out_length)
{
    int data_length = sizeof(ReportDescriptor);
    *out_length = data_length;
    if (length < data_length)
        return STATUS_BUFFER_TOO_SMALL;

    memcpy(buffer, ReportDescriptor, data_length);
    return STATUS_SUCCESS;
}

static NTSTATUS xbox_get_string(DEVICE_OBJECT *device, DWORD index, WCHAR *buffer, DWORD length)
{
    UInt8 idx;
    struct platform_private *ext = get_platform_private(device);

    switch (index)
    {
        case HID_STRING_ID_IPRODUCT:
        {
            (*ext->dev)->USBGetProductStringIndex(ext->dev, &idx);
            return get_device_string(ext->dev, idx, buffer, length);
        }
        case HID_STRING_ID_IMANUFACTURER:
        {
            (*ext->dev)->USBGetManufacturerStringIndex(ext->dev, &idx);
            return get_device_string(ext->dev, idx, buffer, length);
        }
        case HID_STRING_ID_ISERIALNUMBER:
        {
            (*ext->dev)->USBGetSerialNumberStringIndex(ext->dev, &idx);
            return get_device_string(ext->dev, idx, buffer, length);
        }
        default:
            ERR("Unknown string index\n");
            return STATUS_NOT_IMPLEMENTED;
    }

    return STATUS_SUCCESS;
}

static void ReadCompletion(void * HOSTPTR refCon, IOReturn result, void * HOSTPTR arg0)
{
    DEVICE_OBJECT *device = ADDRSPACECAST(DEVICE_OBJECT *, refCon);
    struct platform_private *private = (struct platform_private *)get_platform_private(device);
    UInt32 numBytesRead;
    IN_REPORT *report = (IN_REPORT*)private->buffer;
    /* Invert Y axis */
    if (report->hat_left_y < -32767)
        report->hat_left_y = 32767;
    else
        report->hat_left_y = -1*report->hat_left_y;
    if (report->hat_right_y < -32767)
        report->hat_right_y = 32767;
    else
        report->hat_right_y = -1*report->hat_right_y;


    process_hid_report(device, (BYTE*)report, FIELD_OFFSET(IN_REPORT, reserved));

    numBytesRead = sizeof(private->buffer) - 1;

    (*private->interface)->ReadPipeAsync(private->interface, 1,
                            private->buffer, numBytesRead, ReadCompletion,
                            device);
}

static NTSTATUS xbox_set_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
    struct platform_private *private = (struct platform_private *)get_platform_private(device);
    OUT_REPORT *data;
    IOReturn ret;
    DWORD size;

    size = sizeof(HEADER) + length;
    data = HeapAlloc(GetProcessHeap(), 0, size);
    data->header.cmd = id;
    data->header.size = size;
    memcpy(data->data, report, length);

    ret = (*private->interface)->WritePipe(private->interface, 2, data, size);
    if (ret == kIOReturnSuccess)
    {
        *written = length;
        return STATUS_SUCCESS;
    }
    else
    {
        *written = 0;
        return STATUS_UNSUCCESSFUL;
    }
}

static NTSTATUS xbox_begin_report_processing(DEVICE_OBJECT *object)
{
    struct platform_private *device = (struct platform_private *)get_platform_private(object);
    UInt32 numBytesRead;
    IOReturn ret;

    if (device->started)
        return STATUS_SUCCESS;

    /* Start event queue */
    numBytesRead = sizeof(device->buffer) - 1;
    ret = (*device->interface)->ReadPipeAsync(device->interface, 1,
                            device->buffer, numBytesRead, ReadCompletion,
                            object);
    if (ret != kIOReturnSuccess)
    {
        ERR("Failed Start Device Read Loop\n");
        return STATUS_UNSUCCESSFUL;
    }
    device->started = TRUE;

    return STATUS_SUCCESS;
}

static NTSTATUS xbox_set_output_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
    *written = 0;
    return STATUS_UNSUCCESSFUL;
}

static NTSTATUS xbox_get_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *read)
{
    *read = 0;
    return STATUS_UNSUCCESSFUL;
}

static const platform_vtbl xbox_vtbl = {
    xbox_compare_platform_device,
    xbox_get_reportdescriptor,
    xbox_get_string,
    xbox_begin_report_processing,
    xbox_set_output_report,
    xbox_get_feature_report,
    xbox_set_feature_report,
};

static NTSTATUS open_xbox_gamepad(DEVICE_OBJECT *object)
{
    struct platform_private *private = (struct platform_private *)get_platform_private(object);
    IOReturn ret;
    IOUSBFindInterfaceRequest intf;
    io_iterator_t iter;
    io_service_t usbInterface;
    UInt8 numConfig;
    IOUSBConfigurationDescriptor *desc = NULL;
    IOCFPlugInInterface * HOSTPTR * HOSTPTR plugInInterface = NULL;
    SInt32 score;
    UInt8 pattern;
    ULONG_PTR written;

    if (private->interface != NULL)
        return STATUS_SUCCESS;

    ret = (*private->dev)->GetNumberOfConfigurations(private->dev, &numConfig);
    if (ret != kIOReturnSuccess || numConfig < 1)
        return STATUS_UNSUCCESSFUL;

    ret = (*private->dev)->GetConfigurationDescriptorPtr(private->dev, 0, &desc);
    if (ret != kIOReturnSuccess || desc == NULL)
        return STATUS_UNSUCCESSFUL;

    ret = (*private->dev)->USBDeviceOpen(private->dev);
    if (ret != kIOReturnSuccess)
        return STATUS_UNSUCCESSFUL;

    ret = (*private->dev)->SetConfiguration(private->dev, desc->bConfigurationValue);
    if (ret != kIOReturnSuccess)
    {
        (*private->dev)->USBDeviceClose(private->dev);
        return STATUS_UNSUCCESSFUL;
    }

    /* XBox 360 */
    intf.bInterfaceClass=kIOUSBFindInterfaceDontCare;
    intf.bInterfaceSubClass=93;
    intf.bInterfaceProtocol=1;
    intf.bAlternateSetting=kIOUSBFindInterfaceDontCare;

    ret = (*private->dev)->CreateInterfaceIterator(private->dev, &intf, &iter);
    usbInterface = IOIteratorNext(iter);
    (*private->dev)->USBDeviceClose(private->dev);

    if (!usbInterface)
        return STATUS_UNSUCCESSFUL;

    ret = IOCreatePlugInInterfaceForService(usbInterface,
                                kIOUSBInterfaceUserClientTypeID,
                                kIOCFPlugInInterfaceID,
                                &plugInInterface, &score);

    IOObjectRelease(usbInterface);
    if (ret != kIOReturnSuccess || plugInInterface== NULL)
        return STATUS_UNSUCCESSFUL;

    ret = (*plugInInterface)->QueryInterface(plugInInterface,
                        CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID550),
                        (LPVOID) &private->interface);
    (*plugInInterface)->Release(plugInInterface);

    if (ret != kIOReturnSuccess || !private->interface)
        return STATUS_UNSUCCESSFUL;

    ret = (*private->interface)->USBInterfaceOpen(private->interface);
    if (ret != kIOReturnSuccess)
    {
        (*private->interface)->Release(private->interface);
        return STATUS_UNSUCCESSFUL;
    }

    ret = (*private->interface)->CreateInterfaceAsyncEventSource(private->interface, &private->source);
    if (ret != kIOReturnSuccess)
    {
        (*private->interface)->Release(private->interface);
        return STATUS_UNSUCCESSFUL;
    }
    CFRunLoopAddSource(run_loop, private->source, kCFRunLoopCommonModes);

    /* Turn Off LEDs */
    pattern = 0;
    ret = xbox_set_feature_report(object, 1, &pattern, sizeof(pattern), &written);
    if (ret != STATUS_SUCCESS)
        ERR("Failed to turn off LED\n");

    private->started = FALSE;

    return STATUS_SUCCESS;
}

/*  END HACK ME */

static void cleanupDevice(DEVICE_OBJECT *device)
{
    struct platform_private *ext = (struct platform_private *)get_platform_private(device);
    if (ext->interface) {
        (*ext->interface)->USBInterfaceClose(ext->interface);
        (*ext->interface)->Release(ext->interface);
    }
    if (ext->source) {
        CFRunLoopRemoveSource(run_loop, ext->source, kCFRunLoopCommonModes);
        CFRelease(ext->source);
    }
    IOObjectRelease(ext->object);
    (*ext->dev)->Release(ext->dev);
}

static void process_IOService_Device(io_object_t object)
{
    IOReturn err;
    IOCFPlugInInterface * HOSTPTR * HOSTPTR plugInInterface=NULL;
    IOUSBDeviceInterface500 **dev=NULL;
    SInt32 score;
    HRESULT res;
    DEVICE_OBJECT *device=NULL;
    WORD vid, pid, rel;
    UInt8 class, subclass;
    UInt32 uid;
    UInt8 idx;
    WCHAR serial_string[256];

    ERR("object 0x%x\n",object);

    err = IOCreatePlugInInterfaceForService(object, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);

    if ((kIOReturnSuccess != err) || (plugInInterface == nil) )
    {
        ERR("Unable to create plug in interface for USB device");
        IOObjectRelease(object);
        return;
    }

    res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID500), (LPVOID)&dev);
    (*plugInInterface)->Release(plugInInterface);
    if (res || !dev)
    {
        ERR("Unable to create USB device with QueryInterface\n");
        IOObjectRelease(object);
        return;
    }

    (*dev)->GetLocationID(dev, &uid);

    if (find_device_by_uid(uid))
    {
        WARN("Device ID 0x%04X (%d) already found. Not adding again\n", (DWORD)uid, (DWORD)uid);
        IOObjectRelease(object);
        return;
    }

    (*dev)->GetDeviceVendor(dev, &vid);
    (*dev)->GetDeviceProduct(dev, &pid);
    (*dev)->GetDeviceReleaseNumber(dev, &rel);
    (*dev)->GetDeviceClass(dev, &class);
    (*dev)->GetDeviceSubClass(dev, &subclass);

    (*dev)->USBGetSerialNumberStringIndex(dev, &idx);
    get_device_string(dev, idx, serial_string, 256);
;
    TRACE("Found device VID 0x%04X (%d), PID 0x%04X (%d), release %d, class 0x%04X (%d), subclass 0x%04X (%d), UID 0x%04X (%d), Serial %s\n", vid, vid, pid, pid, rel, class, class, subclass, subclass, (DWORD)uid, (DWORD)uid, debugstr_w(serial_string));

    if (is_xbox_gamepad(vid,pid))
    {
        device = bus_create_hid_device(busidW, vid, pid, -1, 1, uid, serial_string,
                                       1, &xbox_vtbl, sizeof(struct platform_private));
    }
    else
    {
        TRACE("Not an XBOX 360 controller\n");
        return;
    }

    if (!device)
    {
        ERR("Failed to create device\n");
        IOObjectRelease(object);
        (*dev)->Release(dev);
    }
    else
    {
        struct platform_private* ext = get_platform_private(device);
        ext->object = object;
        ext->dev = dev;
        res = open_xbox_gamepad(device);
        if (res != ERROR_SUCCESS)
        {
            cleanupDevice(device);
            IOObjectRelease(object);
            (*dev)->Release(dev);
        }
        else
            IoInvalidateDeviceRelations(device, BusRelations);
    }
}

static void handle_IOServiceMatchingCallback(void * HOSTPTR refcon, io_iterator_t iter)
{
    io_object_t object;

    TRACE("Insertion detected\n");
    while ((object = IOIteratorNext(iter)))
        process_IOService_Device(object);
}

static void handle_IOServiceTerminatedCallback(void * HOSTPTR refcon, io_iterator_t iter)
{
    io_object_t object;

    TRACE("Removal detected\n");
    while ((object = IOIteratorNext(iter)))
    {
        DEVICE_OBJECT *device = NULL;
        device = bus_find_hid_device(&xbox_vtbl, (VOID *)object);
        if (device)
        {
            cleanupDevice(device);
            bus_remove_hid_device(device);
        }
        IOObjectRelease(object);
    }
}

/* This puts the relevent run loop for event handleing into a WINE thread */
static DWORD CALLBACK runloop_thread(VOID *args)
{
    IONotificationPortRef notificationObject;
    CFRunLoopSourceRef notificationRunLoopSource;
    io_iterator_t myIterator;
    io_object_t object;
    CFMutableDictionaryRef dict;

    run_loop = CFRunLoopGetCurrent();

    dict = IOServiceMatching("IOUSBDevice");

    notificationObject = IONotificationPortCreate(kIOMasterPortDefault);
    notificationRunLoopSource = IONotificationPortGetRunLoopSource(notificationObject);
    CFRunLoopAddSource(run_loop, notificationRunLoopSource, kCFRunLoopDefaultMode);

    CFRetain(dict);
    CFRetain(dict);

    IOServiceAddMatchingNotification(notificationObject, kIOMatchedNotification, dict, handle_IOServiceMatchingCallback, NULL, &myIterator);

    while ((object = IOIteratorNext(myIterator)))
        process_IOService_Device(object);

    IOServiceAddMatchingNotification(notificationObject, kIOTerminatedNotification, dict, handle_IOServiceTerminatedCallback, NULL, &myIterator);
    while ((object = IOIteratorNext(myIterator)))
    {
        ERR("Initial removal iteration returned something...\n");
        IOObjectRelease(object);
    }

    CFRunLoopRun();
    TRACE("Run Loop exiting\n");
    return 1;
}

NTSTATUS xbox_driver_init(void)
{
    TRACE("XBOX 360 Driver Init\n");

    run_loop_handle = CreateThread(NULL, 0, runloop_thread, NULL, 0, NULL);

    return STATUS_SUCCESS;
}

#else

NTSTATUS xbox_driver_init(void)
{
    TRACE("Dummy XBOX 360 Driver Init\n");
    return STATUS_SUCCESS;
}

#endif /* HAVE_IOSERVICEMATCHING */

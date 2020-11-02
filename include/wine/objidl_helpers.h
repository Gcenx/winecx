/*
 * File objidl_helpers.h - pass data between 64-bit space and
 * interfaces in 32-bit space.
 *
 * Copyright 2019 Conor McCarthy for Codeweavers.
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

#if defined __IStream_INTERFACE_DEFINED__ && defined COBJMACROS

#ifdef __i386_on_x86_64__

static inline HRESULT i_stream_read(IStream* stream, void * __ptr64 data, ULONG len, ULONG *pread)
{
    *pread = 0;
    while (len)
    {
        HRESULT hr;
        char local[1024];
        DWORD toread = min(len, sizeof(local));
        if ((hr = IStream_Read(stream, local, toread, &toread)) != S_OK)
            return hr;
        if (!toread) break;
        memcpy((char * __ptr64)data + *pread, local, toread);
        len -= toread;
        *pread += toread;
    }
    return S_OK;
}

static inline HRESULT i_stream_write(IStream* stream, const void * __ptr64 data, ULONG len, ULONG *written)
{
    *written = 0;
    while (len)
    {
        HRESULT hr;
        char local[1024];
        DWORD towrite = min(len, sizeof(local));
        memcpy(local, (const char * __ptr64)data + *written, towrite);
        if ((hr = IStream_Write(stream, local, towrite, &towrite)) != S_OK)
            return hr;
        len -= towrite;
        *written += towrite;
    }
    return S_OK;
}

#else

static inline HRESULT i_stream_read(IStream* stream, void *data, ULONG len, ULONG *pread)
{
    return IStream_Read(stream, data, len, pread);
}

static inline HRESULT i_stream_write(IStream* stream, const void *data, ULONG len, ULONG *written)
{
    return IStream_Write(stream, data, len, written);
}

#endif

#endif /* __IStream_INTERFACE_DEFINED_ */
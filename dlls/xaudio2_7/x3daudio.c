/*
 * Copyright (c) 2016 Andrew Eikum for CodeWeavers
 * Copyright (c) 2018 Ethan Lee for CodeWeavers
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

#include "windef.h"
#include "winbase.h"
#include "x3daudio.h"

#include "wine/debug.h"

#include <F3DAudio.h>

#if XAUDIO2_VER >= 8 || defined X3DAUDIO1_VER
WINE_DEFAULT_DEBUG_CHANNEL(xaudio2);
#endif

#ifdef X3DAUDIO1_VER
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, void *pReserved)
{
    TRACE("(%p, %d, %p)\n", hinstDLL, reason, pReserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( hinstDLL );
        break;
    }
    return TRUE;
}
#endif /* X3DAUDIO1_VER */

#if XAUDIO2_VER >= 8
HRESULT CDECL X3DAudioInitialize(UINT32 chanmask, float speedofsound,
        X3DAUDIO_HANDLE handle)
{
    TRACE("0x%x, %f, %p\n", chanmask, speedofsound, handle);
#ifdef HAVE_F3DAUDIOINITIALIZE8
    return F3DAudioInitialize8(chanmask, speedofsound, handle);
#else
    F3DAudioInitialize(chanmask, speedofsound, handle);
    return S_OK;
#endif
}
#endif /* XAUDIO2_VER >= 8 */

#ifdef X3DAUDIO1_VER
#if X3DAUDIO1_VER <= 2
void WINAPI LEGACY_X3DAudioInitialize(UINT32 chanmask, float speedofsound,
        X3DAUDIO_HANDLE handle)
#else
void CDECL LEGACY_X3DAudioInitialize(UINT32 chanmask, float speedofsound,
        X3DAUDIO_HANDLE handle)
#endif
{
    TRACE("0x%x, %f, %p\n", chanmask, speedofsound, handle);
    F3DAudioInitialize(chanmask, speedofsound, handle);
}
#endif /* X3DAUDIO1_VER */

#if XAUDIO2_VER >= 8 || defined X3DAUDIO1_VER
#if defined(X3DAUDIO1_VER) && X3DAUDIO1_VER <= 2
void WINAPI LEGACY_X3DAudioCalculate(const X3DAUDIO_HANDLE handle,
        const X3DAUDIO_LISTENER *listener, const X3DAUDIO_EMITTER *emitter,
        UINT32 flags, X3DAUDIO_DSP_SETTINGS *out)
#else
void CDECL X3DAudioCalculate(const X3DAUDIO_HANDLE handle,
        const X3DAUDIO_LISTENER *listener, const X3DAUDIO_EMITTER *emitter,
        UINT32 flags, X3DAUDIO_DSP_SETTINGS *out)
#endif
{
    TRACE("%p, %p, %p, 0x%x, %p\n", handle, listener, emitter, flags, out);

    #ifndef __i386_on_x86_64__
    F3DAudioCalculate(
        handle,
        (const F3DAUDIO_LISTENER*) listener,
        (const F3DAUDIO_EMITTER*) emitter,
        flags,
        (F3DAUDIO_DSP_SETTINGS*) out
    );
    #else
    {
        /* Since there are pointers in most of these structs, we need to
         * marshal them to match the FAudio struct layouts, which are
         * 64-bit. F3DAUDIO_CONE remains the same between the two. */

        F3DAUDIO_LISTENER fa_listener;
        F3DAUDIO_EMITTER fa_emitter;
        F3DAUDIO_DSP_SETTINGS fa_out;
        F3DAUDIO_DISTANCE_CURVE fa_VolumeCurve, fa_LFECurve,
            fa_LPFDirectCurve, fa_LPFReverbCurve, fa_ReverbCurve;

        #define MARSHAL_EMITTER_CURVE(local_curve, field_name) \
            do { \
                if (emitter->field_name) { \
                    local_curve.PointCount = emitter->field_name->PointCount; \
                    local_curve.pPoints = ADDRSPACECAST(F3DAUDIO_DISTANCE_CURVE_POINT *, emitter->field_name->pPoints); \
                    fa_emitter.field_name = &local_curve; \
                } \
                else { \
                    fa_emitter.field_name = NULL; \
                } \
            } while (0)

        #define MARSHAL_VECTOR(dest, src) \
            do { \
                dest.x = src.x; \
                dest.y = src.y; \
                dest.z = src.z; \
            } while (0)

        MARSHAL_VECTOR(fa_listener.OrientFront, listener->OrientFront);
        MARSHAL_VECTOR(fa_listener.OrientTop, listener->OrientTop);
        MARSHAL_VECTOR(fa_listener.Position, listener->Position);
        MARSHAL_VECTOR(fa_listener.Velocity, listener->Velocity);
        fa_listener.pCone = ADDRSPACECAST(F3DAUDIO_CONE *, listener->pCone);

        MARSHAL_VECTOR(fa_emitter.OrientFront, emitter->OrientFront);
        MARSHAL_VECTOR(fa_emitter.OrientTop, emitter->OrientTop);
        MARSHAL_VECTOR(fa_emitter.Position, emitter->Position);
        MARSHAL_VECTOR(fa_emitter.Velocity, emitter->Velocity);
        fa_emitter.InnerRadius = emitter->InnerRadius;
        fa_emitter.InnerRadiusAngle = emitter->InnerRadiusAngle;
        fa_emitter.ChannelCount = emitter->ChannelCount;
        fa_emitter.ChannelRadius = emitter->ChannelRadius;
        fa_emitter.CurveDistanceScaler = emitter->CurveDistanceScalar;
        fa_emitter.DopplerScaler = emitter->DopplerScalar;
        fa_emitter.pCone = ADDRSPACECAST(F3DAUDIO_CONE *, emitter->pCone);
        fa_emitter.pChannelAzimuths = ADDRSPACECAST(float *, emitter->pChannelAzimuths);
        MARSHAL_EMITTER_CURVE(fa_VolumeCurve, pVolumeCurve);
        MARSHAL_EMITTER_CURVE(fa_LFECurve, pLFECurve);
        MARSHAL_EMITTER_CURVE(fa_LPFDirectCurve, pLPFDirectCurve);
        MARSHAL_EMITTER_CURVE(fa_LPFReverbCurve, pLPFReverbCurve);
        MARSHAL_EMITTER_CURVE(fa_ReverbCurve, pReverbCurve);

        fa_out.SrcChannelCount = out->SrcChannelCount;
        fa_out.DstChannelCount = out->DstChannelCount;
        fa_out.LPFDirectCoefficient = out->LPFDirectCoefficient;
        fa_out.LPFReverbCoefficient = out->LPFReverbCoefficient;
        fa_out.ReverbLevel = out->ReverbLevel;
        fa_out.DopplerFactor = out->DopplerFactor;
        fa_out.EmitterToListenerAngle = out->EmitterToListenerAngle;
        fa_out.EmitterToListenerDistance = out->EmitterToListenerDistance;
        fa_out.EmitterVelocityComponent = out->EmitterVelocityComponent;
        fa_out.ListenerVelocityComponent = out->ListenerVelocityComponent;
        fa_out.pMatrixCoefficients = ADDRSPACECAST(float *, out->pMatrixCoefficients);
        fa_out.pDelayTimes = ADDRSPACECAST(float *, out->pDelayTimes);

        #undef MARSHAL_EMITTER_CURVE
        #undef MARSHAL_VECTOR

        F3DAudioCalculate(
            handle,
            &fa_listener,
            &fa_emitter,
            flags,
            &fa_out
        );

        out->SrcChannelCount = fa_out.SrcChannelCount;
        out->DstChannelCount = fa_out.DstChannelCount;
        out->LPFDirectCoefficient = fa_out.LPFDirectCoefficient;
        out->LPFReverbCoefficient = fa_out.LPFReverbCoefficient;
        out->ReverbLevel = fa_out.ReverbLevel;
        out->DopplerFactor = fa_out.DopplerFactor;
        out->EmitterToListenerAngle = fa_out.EmitterToListenerAngle;
        out->EmitterToListenerDistance = fa_out.EmitterToListenerDistance;
        out->EmitterVelocityComponent = fa_out.EmitterVelocityComponent;
        out->ListenerVelocityComponent = fa_out.ListenerVelocityComponent;
        /* No need to copy back pMatrixCoefficients and pDelayTimes;
         * F3DAudioCalculate may write to those arrays, but it won't
         * change the pointers themselves. */
    }
    #endif
}
#endif /* XAUDIO2_VER >= 8 || defined X3DAUDIO1_VER */

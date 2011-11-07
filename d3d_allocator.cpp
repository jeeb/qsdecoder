/*
 * Copyright (c) 2011, INTEL CORPORATION
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 * Neither the name of INTEL CORPORATION nor the names of its contributors may
 * be used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdafx.h"
#include "QuickSync_defs.h"
#include "d3d_allocator.h"

static const D3DFORMAT D3DFMT_NV12 = (D3DFORMAT)MAKEFOURCC('N','V','1','2');
static const D3DFORMAT D3DFMT_YV12 = (D3DFORMAT)MAKEFOURCC('Y','V','1','2');

D3DFORMAT ConvertMfxFourccToD3dFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return D3DFMT_NV12;
    case MFX_FOURCC_YV12:
        return D3DFMT_YV12;
    case MFX_FOURCC_YUY2:
        return D3DFMT_YUY2;
    case MFX_FOURCC_RGB3:
        return D3DFMT_R8G8B8;
    case MFX_FOURCC_RGB4:
        return D3DFMT_A8R8G8B8;
    case mfxU32(D3DFMT_P8):
        return D3DFMT_P8;
    default:
        return D3DFMT_UNKNOWN;
    }
}

class DeviceHandle
{
public:
    DeviceHandle(IDirect3DDeviceManager9* manager) :
        m_Manager(manager),
        m_Handle(0)
    {
        MSDK_CHECK_POINTER_NO_RET(manager);
        HRESULT hr = manager->OpenDeviceHandle(&m_Handle);
        if (FAILED(hr))
            m_Manager = 0;
    }

    ~DeviceHandle()
    {
        if (m_Manager && m_Handle)
            m_Manager->CloseDeviceHandle(m_Handle);
    }

    HANDLE Detach()
    {
        HANDLE tmp = m_Handle;
        m_Manager = 0;
        m_Handle = 0;
        return tmp;
    }

    inline operator HANDLE()
    {
        return m_Handle;
    }

    inline bool operator!() const
    {
        return m_Handle == 0;
    }

protected:
    CComPtr<IDirect3DDeviceManager9> m_Manager;
    HANDLE m_Handle;
};

D3DFrameAllocator::D3DFrameAllocator() :
    m_DecoderService(0), m_ProcessorService(0), m_hDecoder(0), m_hProcessor(0), m_Manager(0)
{    
}

D3DFrameAllocator::~D3DFrameAllocator()
{
    Close();
}

mfxStatus D3DFrameAllocator::Init(mfxAllocatorParams* pParams)
{   
    D3DAllocatorParams* pd3dParams = 0;
    pd3dParams = dynamic_cast<D3DAllocatorParams* >(pParams);
    MSDK_CHECK_POINTER(pd3dParams, MFX_ERR_NOT_INITIALIZED);
    m_Manager = pd3dParams->pManager;
    return MFX_ERR_NONE;    
}

mfxStatus D3DFrameAllocator::Close()
{   
    if (m_Manager && m_hDecoder)
    {
        m_Manager->CloseDeviceHandle(m_hDecoder);
        m_Manager = 0;
        m_hDecoder = 0;
    }

    if (m_Manager && m_hProcessor)
    {
        m_Manager->CloseDeviceHandle(m_hProcessor);
        m_Manager = 0;
        m_hProcessor = 0;
    }

    return BaseFrameAllocator::Close();
}

mfxStatus D3DFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData* ptr)
{
    IDirect3DSurface9* pSurface = (IDirect3DSurface9*)mid;
    MSDK_CHECK_POINTER(pSurface, MFX_ERR_INVALID_HANDLE);
    MSDK_CHECK_POINTER(ptr, MFX_ERR_LOCK_MEMORY);

    D3DSURFACE_DESC desc;
    HRESULT hr = pSurface->GetDesc(&desc);
    if (FAILED(hr))
        return MFX_ERR_LOCK_MEMORY;

    if (desc.Format != D3DFMT_NV12 &&
        desc.Format != D3DFMT_YV12 &&
        desc.Format != D3DFMT_YUY2 &&
        desc.Format != D3DFMT_R8G8B8 &&
        desc.Format != D3DFMT_A8R8G8B8 &&
        desc.Format != D3DFMT_P8)
        return MFX_ERR_LOCK_MEMORY;

    D3DLOCKED_RECT locked;

    hr = pSurface->LockRect(&locked, NULL, D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK);
    if (FAILED(hr))
        return MFX_ERR_LOCK_MEMORY;

    switch ((DWORD)desc.Format)
    {
    case D3DFMT_NV12:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y = (mfxU8*)locked.pBits;
        ptr->U = (mfxU8*)locked.pBits + desc.Height * locked.Pitch;
        ptr->V = ptr->U + 1;
        break;
    case D3DFMT_YV12:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y = (mfxU8*)locked.pBits;
        ptr->V = ptr->Y + desc.Height * locked.Pitch;
        ptr->U = ptr->V + (desc.Height * locked.Pitch) / 4;
        break;
    case D3DFMT_YUY2:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y = (mfxU8*)locked.pBits;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        break;
    case D3DFMT_R8G8B8:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->B = (mfxU8*)locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        break;
    case D3DFMT_A8R8G8B8:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->B = (mfxU8* )locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
    case D3DFMT_P8:
        ptr->Pitch = (mfxU16)locked.Pitch;
        ptr->Y = (mfxU8*)locked.pBits;
        ptr->U = 0;
        ptr->V = 0;
        break;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData* ptr)
{
    IDirect3DSurface9* pSurface = (IDirect3DSurface9*)mid;
    if (pSurface == 0)
        return MFX_ERR_INVALID_HANDLE;

    pSurface->UnlockRect();
    
    if (NULL != ptr)
    {
        ptr->Pitch = 0;
        ptr->Y     = 0;
        ptr->U     = 0;
        ptr->V     = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL* handle)
{
    if (handle == 0)
        return MFX_ERR_INVALID_HANDLE;

    *handle = mid;
    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::CheckRequestType(mfxFrameAllocRequest* request)
{    
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    if ((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3DFrameAllocator::ReleaseResponse(mfxFrameAllocResponse* response)
{
    if (!response)
        return MFX_ERR_NULL_PTR;

    if (NULL == response->mids)
        return MFX_ERR_NONE;

    mfxStatus sts = MFX_ERR_NONE;

    for (mfxU32 i = 0; i < response->NumFrameActual; i++)
    {
        if (response->mids[i])
        {
            IDirect3DSurface9* handle = 0;
            sts = GetFrameHDL(response->mids[i], (mfxHDL*)&handle);
            if (MFX_ERR_NONE != sts)
                return sts;
            handle->Release();
        }
    }        

    MSDK_SAFE_DELETE_ARRAY(response->mids);
    return sts;
}

mfxStatus D3DFrameAllocator::AllocImpl(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response)
{
    HRESULT hr;
    D3DFORMAT format = ConvertMfxFourccToD3dFormat(request->Info.FourCC);

    if (format == D3DFMT_UNKNOWN)
        return MFX_ERR_UNSUPPORTED;
    
    safe_array<mfxMemId> mids(new mfxMemId[request->NumFrameSuggested]);
    if (!mids.get())
        return MFX_ERR_MEMORY_ALLOC;

    // VPP may require at input/output surfaces with color format other than NV12 (in case of color conversion)
    // VideoProcessorService must be used to create such surfaces
    if (((request->Type & (MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_FROM_VPPOUT)) && (MFX_FOURCC_NV12 != request->Info.FourCC))||
        (request->Type & (MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_FROM_VPPOUT)) && (request->Type & MFX_MEMTYPE_INTERNAL_FRAME ))
    {
        if (!m_hProcessor)
        {
            DeviceHandle device(m_Manager);

            if (!device)
                return MFX_ERR_MEMORY_ALLOC;

            CComPtr<IDirectXVideoProcessorService> service = 0;

            hr = m_Manager->GetVideoService(device, IID_IDirectXVideoProcessorService, (void**)&service);

            if (FAILED(hr))
                return MFX_ERR_MEMORY_ALLOC;

            m_ProcessorService = service;
            m_hProcessor = device.Detach();
        }

        hr = m_ProcessorService->CreateSurface(
            request->Info.Width,
            request->Info.Height,
            request->NumFrameSuggested - 1,
            format,
            D3DPOOL_DEFAULT,
            0,
            DXVA2_VideoProcessorRenderTarget,
            (IDirect3DSurface9**)mids.get(),
            NULL);
    }
    else
    {
        if (!m_hDecoder)
        {
            DeviceHandle device(m_Manager);

            if (!device)
                return MFX_ERR_MEMORY_ALLOC;

            CComPtr<IDirectXVideoDecoderService> service = 0;

            hr = m_Manager->GetVideoService(device, IID_IDirectXVideoDecoderService, (void**)&service);

            if (FAILED(hr))
                return MFX_ERR_MEMORY_ALLOC;

            m_DecoderService = service;
            m_hDecoder = device.Detach();
        }

        hr = m_DecoderService->CreateSurface(
            request->Info.Width,
            request->Info.Height,
            request->NumFrameSuggested - 1,
            format,
            D3DPOOL_DEFAULT,
            0,
            DXVA2_VideoDecoderRenderTarget,
            (IDirect3DSurface9**)mids.get(),
            NULL);
    }

    if (FAILED(hr))
    {        
        return MFX_ERR_MEMORY_ALLOC;
    }

    response->mids = mids.release();
    response->NumFrameActual = request->NumFrameSuggested;
    return MFX_ERR_NONE;
}

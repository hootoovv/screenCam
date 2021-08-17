#pragma warning(disable:4244)
#pragma warning(disable:4711)

#include <string>
#include <streams.h>
#include <stdio.h>
#include <olectl.h>
#include <dvdmedia.h>
#include "common.h"
#include "properties.h"
#include "filters.h"

//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown * WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);
    CUnknown *punk = new CVCam(lpunk, phr);
    return punk;
}

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT *phr) : 
    CSource(NAME("Screen Cam"), lpunk, CLSID_VirtualCam)
{
    ASSERT(phr);
    CAutoLock cAutoLock(&m_cStateLock);
    // Create the one and only output pin
    m_paStreams = (CSourceStream **) new CVCamStream*[1];
    m_paStreams[0] = new CVCamStream(phr, this, L"Screen Cam");
}

HRESULT CVCam::QueryInterface(REFIID riid, void **ppv)
{
    //Forward request for IAMStreamConfig, IKsPropertySet, ISpecifyPropertyPages & IIScreenCam to the pin
    if(riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet) || riid == _uuidof(ISpecifyPropertyPages) || riid == IID_ScreenCam)
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}

static BOOL CALLBACK MonitorEnum(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
{
    CVCamStream* pStream = (CVCamStream*)pData;
    pStream->m_next->m_hMonitor = hMon;
    pStream->m_next->m_rect = *lprcMonitor;

    pStream->m_next++;

    return TRUE;
}

void drawCursor(CURSORINFO* pci, HDC hMemDC)
{
    HICON      icon;
    ICONINFO   ii;
    POINT      win_pos = { 0, 0 };

    if (!(pci->flags & CURSOR_SHOWING))
        return;

    icon = CopyIcon(pci->hCursor);
    if (!icon)
        return;

    if (GetIconInfo(icon, &ii)) {
        POINT pos;

        pos.x = pci->ptScreenPos.x - (int)ii.xHotspot - win_pos.x;
        pos.y = pci->ptScreenPos.y - (int)ii.yHotspot - win_pos.y;

        DrawIconEx(hMemDC, pos.x, pos.y, icon, 0, 0, 0, NULL,
            DI_NORMAL);

        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
    }

    DestroyIcon(icon);
}

DWORD WINAPI CapThreadProc(LPVOID lpParam)
{
    CVCamStream* pStream = (CVCamStream*)lpParam;

    HMONITOR hMonitor = pStream->m_monitors[pStream->m_monitor].m_hMonitor;

    MONITORINFOEX mi;
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfo(hMonitor, &mi);
    std::wstring monitorName = mi.szDevice;

    int imageSize = pStream->m_Width * pStream->m_Height * pStream->m_BPP / 8;
    BITMAPINFO bi = { 0 };
    BITMAPINFOHEADER* bih = &bi.bmiHeader;
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biBitCount = pStream->m_BPP;
    bih->biWidth = pStream->m_Width;
    bih->biHeight = pStream->m_Height;
    bih->biSizeImage = imageSize;
    bih->biPlanes = 1;
    bih->biClrImportant = 0;
    bih->biClrUsed = 0;
    bih->biCompression = 0;
    bih->biXPelsPerMeter = 0;
    bih->biYPelsPerMeter = 0;

    BYTE* bits;
    CURSORINFO ci;
    BOOL cursor_captured = FALSE;
    //DWORD now = 0;

    while (!pStream->m_bStop)
    {
        //now = GetTickCount();

        HDC hDC = ::CreateDC(monitorName.c_str(), monitorName.c_str(), NULL, NULL);
        HDC hMemDC = CreateCompatibleDC(hDC);
        HBITMAP membmp = CreateDIBSection(hMemDC, &bi,
            DIB_RGB_COLORS, (void**)&bits,
            NULL, 0);
        HBITMAP old_bmp = (HBITMAP)SelectObject(hMemDC, membmp);

        memset(&ci, 0, sizeof(CURSORINFO));
        ci.cbSize = sizeof(CURSORINFO);
        if (pStream->m_captureCursor)
            cursor_captured = GetCursorInfo(&ci);
        else
            cursor_captured = FALSE;

        BOOL bRet = ::BitBlt(hMemDC, 0, 0, pStream->m_Width, pStream->m_Height, hDC, 0, 0, SRCCOPY | CAPTUREBLT);

        if (!bRet)
            continue;

        if (cursor_captured)
        {
            drawCursor(&ci, hMemDC);
        }

        memcpy(pStream->m_bmp, bits, imageSize);

        if (hMemDC)
        {
            SelectObject(hMemDC, old_bmp);
            DeleteDC(hMemDC);
            DeleteObject(membmp);
        }

        if (hDC)
        {
            DeleteDC(hDC);
        }

        //DWORD after = GetTickCount();
        //char msg[256];
        //sprintf(msg, "Duration: %ld\n", after - now);
        //OutputDebugStringA(msg);
        //now = after;
    }

    return NOERROR;
}


//////////////////////////////////////////////////////////////////////////
// CVCamStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamStream::CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName) :
    CSourceStream(NAME("Screen Cam"),phr, pParent, pPinName), m_pParent(pParent)
{
    // Set the default media type as 320x240x24@15
    GetMediaType(&m_mt);
    int bmpMemSize = m_Width * m_Height * m_BPP / 8;
    m_bmp = new BYTE[bmpMemSize];
    ZeroMemory(m_bmp, bmpMemSize);

    LoadProfile();
}

CVCamStream::~CVCamStream()
{
    delete[] m_bmp;
} 

HRESULT CVCamStream::QueryInterface(REFIID riid, void **ppv)
{   
    // Standard OLE stuff
    if(riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if(riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else if(riid == _uuidof(ISpecifyPropertyPages))
        *ppv = (ISpecifyPropertyPages*)this;
    else if(riid == IID_ScreenCam)
        *ppv = (IIScreenCam*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef();
    return S_OK;
}

HRESULT CVCamStream::GetPages(CAUUID* pPages)
{
    CheckPointer(pPages, E_POINTER);

    pPages->cElems = 1;
    pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID));
    if (pPages->pElems == NULL) {
        return E_OUTOFMEMORY;
    }

    *(pPages->pElems) = CLSID_VirtualCamProp;
    return NOERROR;

}

//////////////////////////////////////////////////////////////////////////
//  This is the routine where we create the data being output by the Virtual
//  Camera device.
//////////////////////////////////////////////////////////////////////////

HRESULT CVCamStream::FillBuffer(IMediaSample *pms)
{
    REFERENCE_TIME rtNow;
    
    REFERENCE_TIME avgFrameTime = ((VIDEOINFOHEADER*)m_mt.pbFormat)->AvgTimePerFrame;

    rtNow = m_rtLastTime;
    m_rtLastTime += avgFrameTime;
    pms->SetTime(&rtNow, &m_rtLastTime);
    pms->SetSyncPoint(TRUE);

    BYTE *pData;
    long lDataLen;
    pms->GetPointer(&pData);
    lDataLen = pms->GetSize();
    
    memcpy(pData, m_bmp, lDataLen);

    return NOERROR;
} // FillBuffer


//
// Notify
// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CVCamStream::Notify(IBaseFilter * pSender, Quality q)
{
    return E_NOTIMPL;
} // Notify

//////////////////////////////////////////////////////////////////////////
// This is called when the output format has been negotiated
//////////////////////////////////////////////////////////////////////////
HRESULT CVCamStream::SetMediaType(const CMediaType *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamStream::GetMediaType(CMediaType *pmt)
{
    LoadProfile();

    HMONITOR hMonitor = m_monitors[m_monitor].m_hMonitor;

    MONITORINFOEX mi;
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfo(hMonitor, &mi);
    std::wstring monitorName = mi.szDevice;

    HDC hDC = ::CreateDC(monitorName.c_str(), monitorName.c_str(), NULL, NULL);
    m_Width = m_monitors[m_monitor].m_rect.right - m_monitors[m_monitor].m_rect.left;
    m_Height = m_monitors[m_monitor].m_rect.bottom - m_monitors[m_monitor].m_rect.top;

    m_BPP = ::GetDeviceCaps(hDC, BITSPIXEL);

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression  = BI_RGB;
    pvi->bmiHeader.biBitCount     = m_BPP;
    pvi->bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth        = m_Width;
    pvi->bmiHeader.biHeight       = m_Height;
    pvi->bmiHeader.biPlanes       = ::GetDeviceCaps(hDC, PLANES);
    pvi->bmiHeader.biSizeImage    = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = 333333;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&SubTypeGUID);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
    
    if (hDC)
    {
        DeleteDC(hDC);
    }

    return NOERROR;

} // GetMediaType

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CVCamStream::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);

    if(FAILED(hr)) return hr;
    if(Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
} // DecideBufferSize

// Called when graph is run
HRESULT CVCamStream::OnThreadCreate()
{
    m_rtLastTime = 0;
    m_bStop = FALSE;

    m_hThread = CreateThread(NULL, 0, CapThreadProc, this, 0, NULL);

    return NOERROR;
} // OnThreadCreate

// Called when graph is stop running
HRESULT CVCamStream::OnThreadDestroy()
{
    m_bStop = TRUE;

    if (WaitForSingleObject(m_hThread, 500) == WAIT_TIMEOUT)
    {
        TerminateThread(m_hThread, NOERROR);
    }

    CloseHandle(m_hThread);

    return NOERROR;
} // OnThreadDestroy

//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, m_mt.pbFormat);
    m_mt = *pmt;
    IPin* pin; 
    ConnectedTo(&pin);
    if(pin)
    {
        IFilterGraph *pGraph = m_pParent->GetGraph();
        pGraph->Reconnect(this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetFormat(AM_MEDIA_TYPE **ppmt)
{
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetNumberOfCapabilities(int *piCount, int *piSize)
{
    *piCount = 1;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    HMONITOR hMonitor = m_monitors[m_monitor].m_hMonitor;

    MONITORINFOEX mi;
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfo(hMonitor, &mi);
    std::wstring monitorName = mi.szDevice;

    HDC hDC = ::CreateDC(monitorName.c_str(), monitorName.c_str(), NULL, NULL);
    int w = m_monitors[m_monitor].m_rect.right - m_monitors[m_monitor].m_rect.left;
    int h = m_monitors[m_monitor].m_rect.bottom - m_monitors[m_monitor].m_rect.top;

    int bpp = ::GetDeviceCaps(hDC, BITSPIXEL);

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount = bpp;
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = w;
    pvi->bmiHeader.biHeight = h;
    pvi->bmiHeader.biPlanes = ::GetDeviceCaps(hDC, PLANES);
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    if (hDC)
    {
        DeleteDC(hDC);
    }

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    (*pmt)->majortype = MEDIATYPE_Video;
    if (pvi->bmiHeader.biBitCount == 24)
        (*pmt)->subtype = MEDIASUBTYPE_RGB24;
    else if (pvi->bmiHeader.biBitCount == 32)
        (*pmt)->subtype = MEDIASUBTYPE_RGB32;
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bTemporalCompression = FALSE;
    (*pmt)->bFixedSizeSamples= FALSE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);
    
    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);
    
    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = w;
    pvscc->InputSize.cy = h;
    pvscc->MinCroppingSize.cx = 80;
    pvscc->MinCroppingSize.cy = 60;
    pvscc->MaxCroppingSize.cx = w;
    pvscc->MaxCroppingSize.cy = h;
    pvscc->CropGranularityX = 80;
    pvscc->CropGranularityY = 60;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = 80;
    pvscc->MinOutputSize.cy = 60;
    pvscc->MaxOutputSize.cx = w;
    pvscc->MaxOutputSize.cy = h;
    pvscc->OutputGranularityX = 0;
    pvscc->OutputGranularityY = 0;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = 333333;   //30 fps
    pvscc->MaxFrameInterval = 10000000; // 1 fps
    pvscc->MinBitsPerSecond = 80 * 60 * bpp * 8;
    pvscc->MaxBitsPerSecond = w * h * bpp * 8 * 30;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////


HRESULT CVCamStream::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, 
                        DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{// Set: Cannot set any properties.
    return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT CVCamStream::Get(
    REFGUID guidPropSet,   // Which property set.
    DWORD dwPropID,        // Which property in that set.
    void *pInstanceData,   // Instance data (ignore).
    DWORD cbInstanceData,  // Size of the instance data (ignore).
    void *pPropData,       // Buffer to receive the property data.
    DWORD cbPropData,      // Size of the buffer.
    DWORD *pcbReturned     // Return the size of the property.
)
{
    if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;
    
    if (pcbReturned) *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)          return S_OK; // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.
        
    *(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CVCamStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET; 
    return S_OK;
}

HRESULT CVCamStream::get_IScreenCamParams(int* monitor, BOOL* cursor)
{
    CAutoLock cAutolock(&m_camLock);
    CheckPointer(monitor, E_POINTER);
    CheckPointer(cursor, E_POINTER);

    *monitor = m_monitor;
    *cursor = m_captureCursor;

    return NOERROR;
}

HRESULT CVCamStream::put_IScreenCamParams(int monitor, BOOL cursor)
{
    CAutoLock cAutolock(&m_camLock);

    m_monitor = monitor;
    m_captureCursor = cursor;

    SaveProfile();

    return NOERROR;
}

void CVCamStream::LoadProfile()
{
    char szANSI[STR_MAX_LENGTH];

    ::GetProfileStringA("screenCam", "monitor", "0", szANSI, STR_MAX_LENGTH);
    m_monitor = atoi(szANSI);
    
    ::GetProfileStringA("screenCam", "cursor", "1", szANSI, STR_MAX_LENGTH);

    if (strcmp(szANSI, "0") == 0)
        m_captureCursor = FALSE;
    else
        m_captureCursor = TRUE;

    m_monitorCount = ::GetSystemMetrics(SM_CMONITORS);
    if (m_monitor > m_monitorCount - 1)
        m_monitor = 0;

    m_next = &m_monitors[0];
    EnumDisplayMonitors(0, 0, MonitorEnum, (LPARAM) this);
}

void CVCamStream::SaveProfile()
{
    char szANSI[STR_MAX_LENGTH];
    sprintf_s(szANSI, "%d", m_monitor);

    ::WriteProfileStringA("screenCam", "monitor", szANSI);

    if (m_captureCursor)
        strcpy_s(szANSI, "1");
    else
        strcpy_s(szANSI, "0");

    ::WriteProfileStringA("screenCam", "cursor", szANSI);
}



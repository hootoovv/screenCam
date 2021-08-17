#pragma once

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

class CVCamStream;
class CVCam : public CSource
{
public:
    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFilterGraph *GetGraph() {return m_pGraph;}

private:
    CVCam(LPUNKNOWN lpunk, HRESULT *phr);
};

struct MonitorParam {
    HMONITOR m_hMonitor;
    RECT     m_rect;
};

class CVCamStream : public CSourceStream, public IAMStreamConfig, public IKsPropertySet, public ISpecifyPropertyPages, public IIScreenCam
{
public:

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef() { return GetOwner()->AddRef(); }                                                          \
    STDMETHODIMP_(ULONG) Release() { return GetOwner()->Release(); }

    //////////////////////////////////////////////////////////////////////////
    //  IQualityControl
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

    //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE **ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int *piCount, int *piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC);

    //////////////////////////////////////////////////////////////////////////
    //  IKsPropertySet
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData);
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void *pInstanceData,DWORD cbInstanceData, void *pPropData, DWORD cbPropData, DWORD *pcbReturned);
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);
    
    // ISpecifyPropertyPages interface
    STDMETHODIMP GetPages(CAUUID* pPages);

    //////////////////////////////////////////////////////////////////////////
    //  CSourceStream
    //////////////////////////////////////////////////////////////////////////
    CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName);
    ~CVCamStream();

    HRESULT FillBuffer(IMediaSample *pms);
    HRESULT DecideBufferSize(IMemAllocator *pIMemAlloc, ALLOCATOR_PROPERTIES *pProperties);
    HRESULT GetMediaType(CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT OnThreadCreate(void);
    HRESULT OnThreadDestroy(void);

    // These implement the custom IIScreenCam interface
    STDMETHODIMP get_IScreenCamParams(int* monitor, BOOL* cursor);
    STDMETHODIMP put_IScreenCamParams(int monitor, BOOL cursor);

    void LoadProfile();
    void SaveProfile();

    CCritSec    m_camLock;
    int		m_monitor;
    BOOL	m_captureCursor;

    MonitorParam m_monitors[16];
    int      m_monitorCount;
    MonitorParam* m_next;

    BOOL    m_bStop;
    int     m_Width;
    int     m_Height;
    int     m_BPP;
    BYTE*   m_bmp;
    
private:
    CVCam *m_pParent;
    REFERENCE_TIME m_rtLastTime;
    HBITMAP m_hLogoBmp;
    CCritSec m_cSharedState;
    IReferenceClock *m_pClock;

    HANDLE m_hThread;

};



#include <windows.h>
#include <windowsx.h>
#include <streams.h>
#include <commctrl.h>
#include <olectl.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <shlwapi.h>
#include "resource.h"
#include "common.h"
#include "Properties.h"
#include "Filters.h"



//
// CreateInstance
//
// Used by the DirectShow base classes to create instances
//
CUnknown* CVCamProp::CreateInstance(LPUNKNOWN lpunk, HRESULT* phr)
{
    ASSERT(phr);

    CUnknown* punk = new CVCamProp(lpunk);

    if (punk == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    return punk;

} // CreateInstance


//
// Constructor
//
CVCamProp::CVCamProp(IUnknown *pUnk) :
    CBasePropertyPage(NAME("Screen Cam Property Page"), pUnk,
        IDD_PROPERTIES_PAGE, IDS_TITLE),
    m_screenCam(NULL),
    m_bIsInitialized(FALSE)
{
    m_monitor = 0;
    m_captureCursor = TRUE;

} // (Constructor)

CVCamProp::~CVCamProp()
{
}


//
// OnConnect
//
// Called when we connect to a transform filter
//
HRESULT CVCamProp::OnConnect(IUnknown* pUnknown)
{
    CheckPointer(pUnknown, E_POINTER);
    ASSERT(m_screenCam == NULL);

    HRESULT hr = pUnknown->QueryInterface(IID_ScreenCam, (void**)&m_screenCam);
    if (FAILED(hr)) {
        return E_NOINTERFACE;
    }

    //// Get the initial image FX property
    CheckPointer(m_screenCam, E_FAIL);
    m_screenCam->get_IScreenCamParams(&m_monitor, &m_captureCursor);

    m_bIsInitialized = FALSE;
    return NOERROR;

} // OnConnect


//
// OnDisconnect
//
// Likewise called when we disconnect from a filter
//
HRESULT CVCamProp::OnDisconnect()
{
    if (m_screenCam)
    {
        m_screenCam->Release();
        m_screenCam = NULL;
    }
    return NOERROR;

} // OnDisconnect


//
// OnActivate
//
// We are being activated
//
HRESULT CVCamProp::OnActivate()
{
    int monitors = ::GetSystemMetrics(SM_CMONITORS);

    CheckDlgButton(m_Dlg, IDC_CHECK_CURSOR, m_captureCursor);

    TCHAR v[256];

    for (int i = 0; i < monitors; i++)
    {
        (void)StringCchPrintf(v, NUMELMS(v), TEXT("%d\0"), i+1);

        SendDlgItemMessage(m_Dlg, IDC_COMBO_DISPLAY, CB_ADDSTRING, 0, (LPARAM)v);
    }

    if (m_monitor > monitors - 1)
        m_monitor = 0;

    (void)StringCchPrintf(v, NUMELMS(v), TEXT("%d\0"), m_monitor + 1);

    SendDlgItemMessage(m_Dlg, IDC_COMBO_DISPLAY, CB_SELECTSTRING, 0, (LPARAM)v);

    m_bIsInitialized = TRUE;

    return NOERROR;

} // OnActivate


//
// OnDeactivate
//
// We are being deactivated
//
HRESULT CVCamProp::OnDeactivate(void)
{
    m_bIsInitialized = FALSE;

    //if (m_bDirty)
    //{
    //    OnApplyChanges();
    //}

    return NOERROR;

} // OnDeactivate


//
// OnApplyChanges
//
// Apply any changes so far made
//
HRESULT CVCamProp::OnApplyChanges()
{
    GetControlValues();

    CheckPointer(m_screenCam, E_POINTER);
    m_screenCam->put_IScreenCamParams(m_monitor, m_captureCursor);

    return NOERROR;

} // OnApplyChanges


//
// OnReceiveMessage
//
// Handles the messages for our property window
//
INT_PTR CVCamProp::OnReceiveMessage(HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (m_bIsInitialized)
        {
            m_bDirty = TRUE;
            if (m_pPageSite)
            {
                m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
            }
        }
        return (LRESULT)1;
    }

    }

    return CBasePropertyPage::OnReceiveMessage(hwnd, uMsg, wParam, lParam);

} // OnReceiveMessage

void CVCamProp::GetControlValues()
{
    TCHAR v[256];
    ZeroMemory(v, sizeof(v));
    GetDlgItemText(m_Dlg, IDC_COMBO_DISPLAY, v, sizeof(v)-sizeof(TCHAR));

    int monitors = ::GetSystemMetrics(SM_CMONITORS);

#ifdef UNICODE
    char szANSI[STR_MAX_LENGTH];

    int rc = WideCharToMultiByte(CP_ACP, 0, v, -1, szANSI, STR_MAX_LENGTH, NULL, NULL);
    m_monitor = atoi(szANSI) - 1;
#else
    int m_monitor = atoi(szANSI) - 1;
#endif

    if (IsDlgButtonChecked(m_Dlg, IDC_CHECK_CURSOR))
    {
        m_captureCursor = TRUE;
    }
    else
    {
        m_captureCursor = FALSE;
    }
}

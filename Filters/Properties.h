#pragma once
#include "Resource.h"

class CVCamProp : public CBasePropertyPage
{
public:
	CVCamProp(IUnknown *pUnk);
	virtual ~CVCamProp();
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr);

	virtual HRESULT OnConnect(IUnknown* pUnknown);
	virtual HRESULT OnDisconnect();
	virtual HRESULT OnActivate();
	virtual HRESULT OnDeactivate();
	virtual HRESULT OnApplyChanges();
	virtual INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void    GetControlValues();

private:
	BOOL	m_bIsInitialized;      // Used to ignore startup messages

	int		m_monitor;
	BOOL	m_captureCursor;
	IIScreenCam*	m_screenCam;
};

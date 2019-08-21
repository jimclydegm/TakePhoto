#ifndef CAPTURE_H
#define CAPTURE_H

#ifndef UNICODE
#define UNICODE
#endif 

#if !defined( NTDDI_VERSION )
#define NTDDI_VERSION NTDDI_WIN8
#endif

#if !defined( _WIN32_WINNT )
#define _WIN32_WINNT _WIN32_WINNT_WIN8
#endif

#include <new>
#include <windows.h>
#include <windowsx.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfcaptureengine.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <commctrl.h>
#include <d3d11.h>

const UINT WM_APP_CAPTURE_EVENT = WM_APP + 1;

HRESULT CreatePhotoMediaType(IMFMediaType *pSrcMediaType, IMFMediaType **ppPhotoMediaType);

// DXGI DevManager support
extern IMFDXGIDeviceManager* g_pDXGIMan;
extern ID3D11Device*         g_pDX11Device;
extern UINT                  g_ResetToken;

#ifdef _DEBUG
#define DBGMSG(x)  { DbgPrint x;}
#else
#define DBGMSG(x)
#endif

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

// CaptureManager class
// Wraps the capture engine and implements the event callback.

class CaptureManager
{
    // The event callback object.
    class CaptureEngineCB : public IMFCaptureEngineOnEventCallback
    {
        long m_cRef;
        HWND m_hwnd;

    public:
        CaptureEngineCB(HWND hwnd) : m_cRef(1), m_hwnd(hwnd), m_pManager(NULL) {}

        // IUnknown
        STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();
    
        // IMFCaptureEngineOnEventCallback
        STDMETHODIMP OnEvent( _In_ IMFMediaEvent* pEvent);

        CaptureManager* m_pManager;
    };

    HWND                    m_hwndEvent;

    IMFCaptureEngine        *m_pEngine;

    CaptureEngineCB         *m_pCallback;

    CaptureManager(HWND hwnd) : 
        m_hwndEvent(hwnd), m_pEngine(NULL), 
        m_pCallback(NULL)
    {
		// do nothing
    }

public:
    ~CaptureManager()
    {
        DestroyCaptureEngine();
    }

    static HRESULT CreateInstance(HWND hwndEvent, CaptureManager **ppEngine)
    {
        HRESULT hr = S_OK;
        *ppEngine = NULL;

        CaptureManager *pEngine = new (std::nothrow) CaptureManager(hwndEvent);
        if (pEngine == NULL)
        {
            hr = E_OUTOFMEMORY;
            goto Exit;
        }
        *ppEngine = pEngine;
        pEngine = NULL;

    Exit:
        if (NULL != pEngine)
        {
            delete pEngine;
        }
        return hr;
    }

    HRESULT InitializeCaptureManager(IUnknown* pUnk);
    void DestroyCaptureEngine()
    {
        SafeRelease(&m_pEngine);
        SafeRelease(&m_pCallback);

        if(g_pDXGIMan)
        {
            g_pDXGIMan->ResetDevice(g_pDX11Device, g_ResetToken);
        }
        SafeRelease(&g_pDX11Device);
        SafeRelease(&g_pDXGIMan);
    }

    HRESULT TakePhoto(PCWSTR pszFileName);

};

#endif CAPTURE_H

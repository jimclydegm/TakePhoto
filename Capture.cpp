#include "Capture.h"

IMFDXGIDeviceManager* g_pDXGIMan = NULL;
ID3D11Device*         g_pDX11Device = NULL;
UINT                  g_ResetToken = 0;

STDMETHODIMP CaptureManager::CaptureEngineCB::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = 
    {
        QITABENT(CaptureEngineCB, IMFCaptureEngineOnEventCallback),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}      

STDMETHODIMP_(ULONG) CaptureManager::CaptureEngineCB::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CaptureManager::CaptureEngineCB::Release()
{
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

// Callback method to receive events from the capture engine.
STDMETHODIMP CaptureManager::CaptureEngineCB::OnEvent( _In_ IMFMediaEvent* pEvent)
{
    // Post a message to the application window, so the event is handled 
    // on the application's main thread. 

    //if (m_pManager != NULL)
    //{
    //    // We're about to fall asleep, that means we've just asked the CE to stop the preview
    //    // and record.  We need to handle it here since our message pump may be gone.
    //    return S_OK;
    //}
    //else
    //{
    //    pEvent->AddRef();  // The application will release the pointer when it handles the message.
    //    PostMessage(m_hwnd, WM_APP_CAPTURE_EVENT, (WPARAM)pEvent, 0L);
    //}

    return S_OK;
}

HRESULT CreateDX11Device(_Out_ ID3D11Device** ppDevice, _Out_ ID3D11DeviceContext** ppDeviceContext, _Out_ D3D_FEATURE_LEVEL* pFeatureLevel )
{
    HRESULT hr = S_OK;
    static const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,  
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    
    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        ppDevice,
        pFeatureLevel,
        ppDeviceContext
        );
    
    if(SUCCEEDED(hr))
    {
        ID3D10Multithread* pMultithread;
        hr =  ((*ppDevice)->QueryInterface(IID_PPV_ARGS(&pMultithread)));

        if(SUCCEEDED(hr))
        {
            pMultithread->SetMultithreadProtected(TRUE);
        }

        SafeRelease(&pMultithread);
        
    }

    return hr;
}

HRESULT CreateD3DManager()
{
    HRESULT hr = S_OK;
    D3D_FEATURE_LEVEL FeatureLevel;
    ID3D11DeviceContext* pDX11DeviceContext;
    
    hr = CreateDX11Device(&g_pDX11Device, &pDX11DeviceContext, &FeatureLevel);

    if(SUCCEEDED(hr))
    {
        hr = MFCreateDXGIDeviceManager(&g_ResetToken, &g_pDXGIMan);
    }

    if(SUCCEEDED(hr))
    {
        hr = g_pDXGIMan->ResetDevice(g_pDX11Device, g_ResetToken);
    }
    
    SafeRelease(&pDX11DeviceContext);
    
    return hr;
}

HRESULT
CaptureManager::InitializeCaptureManager(IUnknown* pUnk)
{
    HRESULT                         hr = S_OK;
    IMFAttributes*                  pAttributes = NULL;
    IMFCaptureEngineClassFactory*   pFactory = NULL;

    DestroyCaptureEngine();

    //m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    //if (NULL == m_hEvent)
    //{
    //    hr = HRESULT_FROM_WIN32(GetLastError());
    //    goto Exit;
    //}

    m_pCallback = new (std::nothrow) CaptureEngineCB(m_hwndEvent);
    if (m_pCallback == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    m_pCallback->m_pManager = this;

    //Create a D3D Manager
    hr = CreateD3DManager();
    if (FAILED(hr))
    {
        goto Exit;
    }
    hr = MFCreateAttributes(&pAttributes, 1); 
    if (FAILED(hr))
    {
        goto Exit;
    }
    hr = pAttributes->SetUnknown(MF_CAPTURE_ENGINE_D3D_MANAGER, g_pDXGIMan);
    if (FAILED(hr))
    {
        goto Exit;
    }

    // Create the factory object for the capture engine.
    hr = CoCreateInstance(CLSID_MFCaptureEngineClassFactory, NULL, 
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr))
    {
        goto Exit;
    }

    // Create and initialize the capture engine.
    hr = pFactory->CreateInstance(CLSID_MFCaptureEngine, IID_PPV_ARGS(&m_pEngine));
    if (FAILED(hr))
    {
        goto Exit;
    }
    hr = m_pEngine->Initialize(m_pCallback, pAttributes, NULL, pUnk);
    if (FAILED(hr))
    {
        goto Exit;
    }

Exit:
    if (NULL != pAttributes)
    {
        pAttributes->Release();
        pAttributes = NULL;
    }
    if (NULL != pFactory)
    {
        pFactory->Release();
        pFactory = NULL;
    }
    return hr;
}

HRESULT CaptureManager::TakePhoto(PCWSTR pszFileName)
{
    IMFCaptureSink *pSink = NULL;
    IMFCapturePhotoSink *pPhoto = NULL;
    IMFCaptureSource *pSource;
    IMFMediaType *pMediaType = 0;
    IMFMediaType *pMediaType2 = 0;
    bool bHasPhotoStream = true;

    // Get a pointer to the photo sink.
    HRESULT hr = m_pEngine->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PHOTO, &pSink);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pSink->QueryInterface(IID_PPV_ARGS(&pPhoto));
    if (FAILED(hr))
    {
        goto done;
    }

    hr = m_pEngine->GetSource(&pSource);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pSource->GetCurrentDeviceMediaType((DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_PHOTO , &pMediaType);     
    if (FAILED(hr))
    {
        goto done;
    }

    //Configure the photo format
    hr = CreatePhotoMediaType(pMediaType, &pMediaType2);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pPhoto->RemoveAllStreams();
    if (FAILED(hr))
    {
        goto done;
    }

    DWORD dwSinkStreamIndex;
    // Try to connect the first still image stream to the photo sink
    if(bHasPhotoStream)
    {
        hr = pPhoto->AddStream((DWORD)MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_PHOTO,  pMediaType2, NULL, &dwSinkStreamIndex);        
    }    

    if(FAILED(hr))
    {
        goto done;
    }

    hr = pPhoto->SetOutputFileName(pszFileName);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = m_pEngine->TakePhoto();
    if (FAILED(hr))
    {
        goto done;
    }

done:
    SafeRelease(&pSink);
    SafeRelease(&pPhoto);
    SafeRelease(&pSource);
    SafeRelease(&pMediaType);
    SafeRelease(&pMediaType2);
    return hr;
}





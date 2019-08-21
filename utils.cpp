#include "Capture.h"
#include <wincodec.h>

HRESULT CopyAttribute(IMFAttributes *pSrc, IMFAttributes *pDest, const GUID& key)
{
    PROPVARIANT var;
    PropVariantInit( &var );
    HRESULT hr = pSrc->GetItem(key, &var);
    if (SUCCEEDED(hr))
    {
        hr = pDest->SetItem(key, var);
        PropVariantClear(&var);
    }
    return hr;
}

// Creates a JPEG image type that is compatible with a specified video media type.

HRESULT CreatePhotoMediaType(IMFMediaType *pSrcMediaType, IMFMediaType **ppPhotoMediaType)
{
    *ppPhotoMediaType = NULL;

    const UINT32 uiFrameRateNumerator = 30;
    const UINT32 uiFrameRateDenominator = 1;

    IMFMediaType *pPhotoMediaType = NULL;

    HRESULT hr = MFCreateMediaType(&pPhotoMediaType);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pPhotoMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Image);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pPhotoMediaType->SetGUID(MF_MT_SUBTYPE, GUID_ContainerFormatJpeg);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = CopyAttribute(pSrcMediaType, pPhotoMediaType, MF_MT_FRAME_SIZE);
    if (FAILED(hr))
    {
        goto done;
    }

    *ppPhotoMediaType = pPhotoMediaType;
    (*ppPhotoMediaType)->AddRef();

done:
    SafeRelease(&pPhotoMediaType);
    return hr;
}
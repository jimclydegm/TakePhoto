#include "Capture.h"
#include <shlobj.h>

CaptureManager *g_pEngine = NULL;

void OnTakePhotos()
{
	wchar_t filename[MAX_PATH];

	// Get the path to the Documents folder.
	IShellItem *psi = NULL;
	PWSTR pszFolderPath = NULL;
	wchar_t PhotoFileName[MAX_PATH];

	HRESULT hr = SHCreateItemInKnownFolder(FOLDERID_Documents, 0, NULL, IID_PPV_ARGS(&psi));
	if (FAILED(hr))
	{
		goto done;
	}

	hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath);
	if (FAILED(hr))
	{
		goto done;
	}

	// Construct a file name based on the current time.

	SYSTEMTIME time;
	GetLocalTime(&time);

	hr = StringCchPrintf(filename, MAX_PATH, L"MyPhoto%04u_%02u%02u_%02u%02u%02u.jpg",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	if (FAILED(hr))
	{
		goto done;
	}

	LPTSTR path = PathCombine(PhotoFileName, pszFolderPath, filename);
	if (path == NULL)
	{
		hr = E_FAIL;
		goto done;
	}

	hr = g_pEngine->TakePhoto(path);
	if (FAILED(hr))
	{
		goto done;
	}

done:
	SafeRelease(&psi);
	CoTaskMemFree(pszFolderPath);
}


void OnCreateEngine()
{
	BOOL                fSuccess = FALSE;
	IMFAttributes*      pAttributes = NULL;
	HRESULT             hr = S_OK;
	HWND hwnd = NULL;
	HWND hPreview = NULL;
	IMFActivate* pSelectedDevice = NULL;

	if (FAILED(CaptureManager::CreateInstance(hwnd, &g_pEngine)))
	{
		goto done;
	}

	hr = g_pEngine->InitializeCaptureManager(pSelectedDevice);
	if (FAILED(hr))
	{
		goto done;
	}

	fSuccess = TRUE;

done:

	SafeRelease(&pAttributes);
}

INT WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/, _In_ LPWSTR /*lpCmdLine*/, _In_ INT nCmdShow)
{
    bool bCoInit = false, bMFStartup = false;
    // Note: The shell common File dialog requires apartment threading.
	HRESULT hr = NULL;
    bCoInit = true;

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        goto done;
    }

    bMFStartup = true;

	// periodic timer
	HANDLE m_hTakePhoto = CreateWaitableTimer(NULL, FALSE, NULL);;
	LARGE_INTEGER liDueTime;
	LONG lPeriod;
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	memcpy(&liDueTime, &ft, sizeof(LARGE_INTEGER));
	lPeriod = 3 * 1000;

	SetWaitableTimer(m_hTakePhoto, &liDueTime, lPeriod, NULL, NULL, 0);

	HANDLE hToWait[] = {
		m_hTakePhoto
	};

	bool bCont = true;
	int ctr = 0;
	OnCreateEngine();

	while (bCont)
	{
		DWORD dwRetVal = MsgWaitForMultipleObjects(ARRAYSIZE(hToWait), hToWait, FALSE,
			INFINITE, QS_ALLEVENTS | QS_SENDMESSAGE);

		switch (dwRetVal)
		{
		case WAIT_OBJECT_0:		// take photo
		{
			OnTakePhotos();
			if (ctr++ == 6)
			{
				bCont = false;
			}
			break;
		}

		case WAIT_TIMEOUT:
			break;

		default:
			bCont = false;
			break;
		}
	}


done:
    if (bMFStartup)
    {
        MFShutdown();
    }
    if (bCoInit)
    {
        CoUninitialize();
    }
    return 0;
}
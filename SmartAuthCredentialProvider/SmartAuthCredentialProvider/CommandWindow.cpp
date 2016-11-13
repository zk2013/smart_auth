//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//

#include "CommandWindow.h"
#include <strsafe.h>
#include "SerialClass.h"
#include <Windows.h>
#include "donglein.h"

// Custom messages for managing the behavior of the window thread.
#define WM_EXIT_THREAD              WM_USER + 1
#define WM_TOGGLE_CONNECTED_STATUS  WM_USER + 2

const TCHAR c_szClassName[] = TEXT("EventWindow");
const TCHAR c_szConnected[] = TEXT("Connected");
const TCHAR c_szDisconnected[] = TEXT("Disconnected");

CCommandWindow::CCommandWindow() : _hWnd(NULL), _hInst(NULL), _fConnected(FALSE), _pProvider(NULL)
{
}

CCommandWindow::~CCommandWindow()
{
    // If we have an active window, we want to post it an exit message.
    if (_hWnd != NULL)
    {
        PostMessage(_hWnd, WM_EXIT_THREAD, 0, 0);
        _hWnd = NULL;
    }

    // We'll also make sure to release any reference we have to the provider.
    if (_pProvider != NULL)
    {
        _pProvider->Release();
        _pProvider = NULL;
    }

	if (hThread != INVALID_HANDLE_VALUE) {
		TerminateThread(hThread, 0);
	}
}

// Performs the work required to spin off our message so we can listen for events.
HRESULT CCommandWindow::Initialize(__in SmartAuthProvider *pProvider)
{
    HRESULT hr = S_OK;

    // Be sure to add a release any existing provider we might have, then add a reference
    // to the provider we're working with now.
    if (_pProvider != NULL)
    {
        _pProvider->Release();
    }
    _pProvider = pProvider;
    _pProvider->AddRef();
    
    // Create and launch the window thread.
    // HANDLE hThread = CreateThread(NULL, 0, _ThreadProc, this, 0, NULL);
	hThread = CreateThread(NULL, 0, _ThreadProc2, this, 0, NULL);
    if (hThread == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

	HKEY phkResult;
	RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\SmartAuth\\", REG_OPTION_OPEN_LINK, KEY_ALL_ACCESS, &phkResult);

	BYTE data[256];
	memset(data, 0, sizeof(data));
	DWORD cbData = 256;
	RegQueryValueExW(phkResult, L"Donglein", NULL, NULL, data, &cbData);

	if (lstrcmpW((LPCWSTR)data, L"0") == 0) {
		donglein = FALSE;
	}
	else if (lstrcmpW((LPCWSTR)data, L"1") == 0) {
		donglein = TRUE;
	}

	memset(data, 0, sizeof(data));
	cbData = 256;
	RegQueryValueExW(phkResult, L"DongleinKey", NULL, NULL, data, &cbData);

	memset(donglein_key, 0, sizeof(donglein_key));
	WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)data, -1, donglein_key, sizeof(donglein_key), NULL, NULL);

	memset(data, 0, sizeof(data));
	cbData = 256;
	RegQueryValueExW(phkResult, L"SmartIDCard", NULL, NULL, data, &cbData);

	if (lstrcmpW((LPCWSTR)data, L"0") == 0) {
		smart_id_card = FALSE;
	}
	else if (lstrcmpW((LPCWSTR)data, L"1") == 0) {
		smart_id_card = TRUE;
	}

	RegCloseKey(phkResult);

    return hr;
}

// Wraps our internal connected status so callers can easily access it.
BOOL CCommandWindow::GetConnectedStatus()
{
    return _fConnected;
}

//
//  FUNCTION: _MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
HRESULT CCommandWindow::_MyRegisterClass()
{
    WNDCLASSEX wcex = { sizeof(wcex) };
    wcex.style            = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc      = _WndProc;
    wcex.hInstance        = _hInst;
    wcex.hIcon            = NULL;
    wcex.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground    = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName    = c_szClassName;

    return RegisterClassEx(&wcex) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

//
//   FUNCTION: _InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HRESULT CCommandWindow::_InitInstance()
{
    HRESULT hr = S_OK;

    // Create our window to receive events.
    // 
    // This dialog is for demonstration purposes only.  It is not recommended to create 
    // dialogs that are visible even before a credential enumerated by this credential 
    // provider is selected.  Additionally, any dialogs that are created by a credential
    // provider should not have a NULL hwndParent, but should be parented to the HWND
    // returned by ICredentialProviderCredentialEvents::OnCreatingWindow.
    _hWnd = CreateWindowEx(
        WS_EX_TOPMOST, 
        c_szClassName, 
        c_szDisconnected, 
        WS_DLGFRAME,
        200, 200, 200, 80, 
        NULL,
        NULL, _hInst, NULL);
    if (_hWnd == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    if (SUCCEEDED(hr))
    {
        // Add a button to the window.
        _hWndButton = CreateWindow(TEXT("Button"), TEXT("Press to connect"), 
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
                             10, 10, 180, 30, 
                             _hWnd, 
                             NULL,
                             _hInst,
                             NULL);
        if (_hWndButton == NULL)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }

        if (SUCCEEDED(hr))
        {
            // Show and update the window.
            if (!ShowWindow(_hWnd, SW_NORMAL))
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
            }

            if (SUCCEEDED(hr))
            {
                if (!UpdateWindow(_hWnd))
                {
                   hr = HRESULT_FROM_WIN32(GetLastError());
                }
            }
        }
    }

    return hr;
}

// Called from the separate thread to process the next message in the message queue. If
// there are no messages, it'll wait for one.
BOOL CCommandWindow::_ProcessNextMessage()
{
    // Grab, translate, and process the message.
    MSG msg;
    GetMessage(&(msg), _hWnd, 0, 0);
    TranslateMessage(&(msg));
    DispatchMessage(&(msg));

    // This section performs some "post-processing" of the message. It's easier to do these
    // things here because we have the handles to the window, its button, and the provider
    // handy.
    switch (msg.message)
    {
    // Return to the thread loop and let it know to exit.
    case WM_EXIT_THREAD: return FALSE;

    // Toggle the connection status, which also involves updating the UI.
    case WM_TOGGLE_CONNECTED_STATUS:
        _fConnected = !_fConnected;
        if (_fConnected)
        {
            SetWindowText(_hWnd, c_szConnected);
            SetWindowText(_hWndButton, TEXT("Press to disconnect"));
        }
        else
        {
            SetWindowText(_hWnd, c_szDisconnected);
            SetWindowText(_hWndButton, TEXT("Press to connect"));
        }
        _pProvider->OnConnectStatusChanged();
        break;
    }
    return TRUE;
}

BOOL CCommandWindow::SubProc(Serial* SP) {
	BOOL is_donglein_connected = FALSE;
	BOOL is_smart_id_card_connected = FALSE;

	if (donglein == TRUE) {
		HANDLE hUsb;
		CAPACITY mediaCapacity;
		BYTE Context[512] = { 0 };

		hUsb = CreateDevice();

		if (hUsb == INVALID_HANDLE_VALUE)
		{
			if (_fConnected) {
				_fConnected = !_fConnected;
				_pProvider->OnConnectStatusChanged();
			}

			Sleep(200);

			return TRUE;
		}

		if (GetConfigurationDescriptor(hUsb) == FALSE)
		{
			if (_fConnected) {
				_fConnected = !_fConnected;
				_pProvider->OnConnectStatusChanged();
			}

			Sleep(200);

			return TRUE;
		}

		int count = 0;
		while (ReadCapacity(hUsb, &mediaCapacity) == FALSE) {
			RequestSense(hUsb);
			Sleep(100);
			if (count > 20) {
				if (_fConnected) {
					_fConnected = !_fConnected;
					_pProvider->OnConnectStatusChanged();
				}
				return TRUE;
			}
			count++;
		}

		MediaRead(hUsb, &mediaCapacity, Context);

		CHAR t[512] = { 0 };
		for (int i = 300; i < 316; i++) {
			t[i - 300] = Context[i];
		}

		if (strcmp(t, donglein_key) == 0) {
			is_donglein_connected = TRUE;
		}
		else {
			is_donglein_connected = FALSE;
		}

		CloseHandle(hUsb);
	}
	
	if (smart_id_card == TRUE) {
		SP->WriteData("a\r", 2);

		Sleep(200);

		char incomingData[256] = "";
		int dataLength = 255;
		int readResult = 0;
		readResult = SP->ReadData(incomingData, dataLength);
		incomingData[readResult] = 0;

		if (strcmp(incomingData, "b") == 0) {
			is_smart_id_card_connected = TRUE;
		}
		else {
			is_smart_id_card_connected = FALSE;
		}
	}

	BOOL t2 = TRUE;

	if (donglein == TRUE && is_donglein_connected == FALSE) {
		t2 = FALSE;
	}

	if (smart_id_card == TRUE && is_smart_id_card_connected == FALSE) {
		t2 = FALSE;
	}

	if (t2 == TRUE && !_fConnected) {
		_fConnected = !_fConnected;
		_pProvider->OnConnectStatusChanged();
	}
	else if (t2 == FALSE && _fConnected) {
		_fConnected = !_fConnected;
		_pProvider->OnConnectStatusChanged();
	}
	
	Sleep(200);

	return TRUE;
}

// Manages window messages on the window thread.
LRESULT CALLBACK CCommandWindow::_WndProc(__in HWND hWnd, __in UINT message, __in WPARAM wParam, __in LPARAM lParam)
{
    switch (message)
    {
    // Originally we were going to work with USB keys being inserted and removed, but it
    // seems as though these events don't get to us on the secure desktop. However, you
    // might see some messageboxi in CredUI.
    // TODO: Remove if we can't use from LogonUI.
    case WM_DEVICECHANGE:
        MessageBox(NULL, TEXT("Device change"), TEXT("Device change"), 0);
        break;

    // We assume this was the button being clicked.
    case WM_COMMAND:
        PostMessage(hWnd, WM_TOGGLE_CONNECTED_STATUS, 0, 0);
        break;

    // To play it safe, we hide the window when "closed" and post a message telling the 
    // thread to exit.
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        PostMessage(hWnd, WM_EXIT_THREAD, 0, 0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Our thread procedure. We actually do a lot of work here that could be put back on the 
// main thread, such as setting up the window, etc.
DWORD WINAPI CCommandWindow::_ThreadProc(__in LPVOID lpParameter)
{
    CCommandWindow *pCommandWindow = static_cast<CCommandWindow *>(lpParameter);
    if (pCommandWindow == NULL)
    {
        // TODO: What's the best way to raise this error?
        return 0;
    }

    HRESULT hr = S_OK;

    // Create the window.
    pCommandWindow->_hInst = GetModuleHandle(NULL);
    if (pCommandWindow->_hInst != NULL)
    {            
        hr = pCommandWindow->_MyRegisterClass();
        if (SUCCEEDED(hr))
        {
            hr = pCommandWindow->_InitInstance();
        }
    }
    else
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    // ProcessNextMessage will pump our message pump and return false if it comes across
    // a message telling us to exit the thread.
    if (SUCCEEDED(hr))
    {        
        while (pCommandWindow->_ProcessNextMessage()) 
        {
        }
    }
    else
    {
        if (pCommandWindow->_hWnd != NULL)
        {
            pCommandWindow->_hWnd = NULL;
        }
    }

    return 0;
}

DWORD WINAPI CCommandWindow::_ThreadProc2(__in LPVOID lpParameter) {
	CCommandWindow *pCommandWindow = static_cast<CCommandWindow *>(lpParameter);
	if (pCommandWindow == NULL) {
		return 0;
	}
	
	Serial* SP = new Serial("\\\\.\\COM3");
	
	while (pCommandWindow->SubProc(SP)) {
	}
}

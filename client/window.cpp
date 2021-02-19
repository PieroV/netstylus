/**
 * Window management for NetStylus
 *
 * Written in 2021 by Pier Angelo Vendrame <vogliadifarniente AT gmail DOT com>
 *
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * If your country does not recognize the public domain, or if you need a
 * license, please refer to Creative Commons CC0 Public Domain Dedication
 * <http://creativecommons.org/publicdomain/zero/1.0>.
 */

/**
 * \file
 * This file contains the class that manages the NetStylus window.
 */

#define NOMINMAX

#include "stylus_manager.h"

#include <windows.h>
#include <ole2.h>

#include <limits>
#include <cstdio>

class NetStylusApp {
	NetStylusApp() = default;
	NetStylusApp(const NetStylusApp &) = delete;
	NetStylusApp(NetStylusApp &&) = delete;
	NetStylusApp &operator=(const NetStylusApp &) = delete;
	NetStylusApp &operator=(NetStylusApp &&) = delete;

public:
	static NetStylusApp &get();
	int run();

private:
	void registerClasses();

	bool createMainWindow();
	LRESULT mainHandler(HWND window, UINT msg, WPARAM param, LPARAM lparam);

	void setupOpen();
	LRESULT setupHandler(HWND window, UINT msg, WPARAM param, LPARAM lparam);
	bool setupConnect();

	static LRESULT mainHandlerProxy(HWND window, UINT msg, WPARAM param,
		LPARAM lparam);
	static LRESULT setupHandlerProxy(HWND window, UINT msg, WPARAM param,
		LPARAM lparam);

	HINSTANCE mInstance = nullptr;

	HWND mMainWindow = nullptr;
	HWND mSetupWindow = nullptr;

	HWND mEditHost = nullptr;
	HWND mEditPort = nullptr;

	StylusManager mManager;
};


// int main()
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine,
	INT nCmdShow)
{
	return NetStylusApp::get().run();
}


static const char MAIN_CLASS[] = "NetStylus";
static const char SETUP_CLASS[] = "NetStylusSetup";

static const int menuSetup = 1;
static const unsigned int setupOk = 2;
static const unsigned int setupCancel = 3;


NetStylusApp &NetStylusApp::get()
{
	static NetStylusApp instance;
	return instance;
}

int NetStylusApp::run()
{
	mInstance = static_cast<HINSTANCE>(GetModuleHandle(nullptr));

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2,2), &wsa)) {
		printf("Could not start Winsock: %d\n", WSAGetLastError());
		return 1;
	}

	registerClasses();
	if (!createMainWindow()) {
		puts("Could not create the main window");
		return 2;
	}
	ShowWindow(mMainWindow, SW_SHOWDEFAULT);

	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	WSACleanup();

	return 0;
}

void NetStylusApp::registerClasses()
{
	{
		WNDCLASSEX wc = {};
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.lpfnWndProc = &NetStylusApp::mainHandlerProxy;
		wc.lpszClassName = MAIN_CLASS;
		wc.hInstance = mInstance;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.style = CS_HREDRAW | CS_VREDRAW;

		RegisterClassEx(&wc);
	}

	{
		WNDCLASSEX wc = {};
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.lpfnWndProc = &NetStylusApp::setupHandlerProxy;
		wc.lpszClassName = SETUP_CLASS;
		wc.hInstance = mInstance;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.style = CS_HREDRAW | CS_VREDRAW;

		RegisterClassEx(&wc);
	}
}

bool NetStylusApp::createMainWindow()
{
	mMainWindow = CreateWindowEx(
		0, MAIN_CLASS, "NetStylus", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr, nullptr, mInstance, this);
	return mMainWindow;
}

LRESULT NetStylusApp::mainHandler(HWND window, UINT msg, WPARAM param,
	LPARAM lparam)
{
	switch (msg) {
	case WM_CREATE:
		if (mManager.setup(window)) {
			mManager.handler->windowChanged(window);
		}
		{
			HMENU menubar = CreateMenu();
			AppendMenu(menubar, MF_STRING, menuSetup, "Setup");
			SetMenu(window, menubar);
		}
		break;

	case WM_SIZE:
	case WM_MOVE:
		if (mManager.handler) {
			mManager.handler->windowChanged(window);
		}
		break;

	case WM_COMMAND:
		if (param == menuSetup) {
			setupOpen();
		}
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(window, &ps);
			FillRect(dc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW+1));
			EndPaint(window, &ps);
			return 0;
		}

	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO size = reinterpret_cast<LPMINMAXINFO>(lparam);
			size->ptMinTrackSize.x = 320;
			size->ptMinTrackSize.y = 180;
		}
		return 0;

	case WM_DESTROY:
		mManager.reset();
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(window, msg, param, lparam);
}

void NetStylusApp::setupOpen()
{
	mSetupWindow = CreateWindowEx(
		0, SETUP_CLASS, "Setup", WS_DLGFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT, 270, 120,
		nullptr, nullptr, mInstance, nullptr);
	if (!mSetupWindow) {
		puts("Could not open the setup window");
		return;
	}

	mEditHost = CreateWindow("Edit", "Host",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_GROUP | ES_LEFT,
		10, 12, 150, 20, mSetupWindow, nullptr, mInstance, nullptr);
	mEditPort = CreateWindow("Edit", "Port",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
		10, 47, 80, 20, mSetupWindow, nullptr, mInstance, nullptr);
	CreateWindow("Button", "OK",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		170, 10, 70, 24, mSetupWindow, reinterpret_cast<HMENU>(setupOk),
		mInstance, nullptr);
	CreateWindow("Button", "Cancel",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		170, 45, 70, 24, mSetupWindow, reinterpret_cast<HMENU>(setupCancel),
		mInstance, nullptr);

	ShowWindow(mSetupWindow, SW_SHOWDEFAULT);
}

LRESULT NetStylusApp::setupHandler(HWND window, UINT msg, WPARAM param,
	LPARAM lparam)
{
	switch (msg) {
	case WM_CREATE:
		EnableWindow(mMainWindow, FALSE);
		break;

	case WM_COMMAND:
		if (param == setupOk) {
			if (!setupConnect()) {
				break;
			}
		} else if (param != setupCancel) {
			break;
		}

	case WM_DESTROY:
		EnableWindow(mMainWindow, TRUE);
		DestroyWindow(window);
		window = nullptr;
		return 0;
	}
	return DefWindowProc(window, msg, param, lparam);
}

bool NetStylusApp::setupConnect()
{
	if (!mManager.handler) {
		return true;
	}

	const size_t portLen = 6;
	char portStr[portLen];
	GetWindowText(mEditPort, portStr, portLen);
	char *portEnd;
	long port = strtol(portStr, &portEnd, 10);
	if (portEnd == portStr || *portEnd
			|| port > std::numeric_limits<uint16_t>::max()) {
		MessageBox(mSetupWindow, "The port must be a number", "Invalid port",
			MB_OK | MB_ICONERROR);
		return false;
	}

	const size_t hostLen = 256;
	char host[hostLen];
	GetWindowText(mEditHost, host, hostLen);
	if (!mManager.handler->setServer(host, static_cast<uint16_t>(port))) {
		MessageBox(mSetupWindow, "Could not find the host", "Invalid host",
			MB_OK | MB_ICONERROR);
		return false;
	}

	return true;
}

LRESULT NetStylusApp::mainHandlerProxy(HWND window, UINT msg,
	WPARAM param, LPARAM lparam)
{
	return get().mainHandler(window, msg, param, lparam);
}

LRESULT NetStylusApp::setupHandlerProxy(HWND window, UINT msg,
	WPARAM param, LPARAM lparam)
{
	return get().setupHandler(window, msg, param, lparam);
}

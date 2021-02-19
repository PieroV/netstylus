/**
 * IRealTimeStylus manager for NetStylus
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
 * This file contains code for initialization of the time stylus support.
 *
 * It is based on the code from https://backworlds.com/under-pressure/ .
 */

#include "stylus_manager.h"

StylusManager::~StylusManager()
{
	reset();
}

bool StylusManager::setup(HWND hWnd)
{
	HRESULT hr;
	reset();

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		printf("Could not initialize COM: %lu\n", hr);
		return false;
	}

	// Create stylus
	hr = CoCreateInstance(CLSID_RealTimeStylus, nullptr, CLSCTX_ALL,
		IID_PPV_ARGS(&stylus));
	if (FAILED(hr)) {
		printf("Could not create the stylus: %lu\n", hr);
		return false;
	}

	// Attach RTS object to a window
	hr = stylus->put_HWND((HANDLE_PTR)hWnd);
	if (FAILED(hr)) {
		puts("Could not attach the stylus to the window");
		reset();
		return false;
	}

	// Create eventhandler
	handler = new NetworkStylus(stylus);

	// Create free-threaded marshaller for this object and aggregate it.
	hr = handler->createMarshaller();
	if (FAILED(hr)) {
		puts("Could not create the marshaller");
		reset();
		return false;
	}

	// Add handler object to the list of synchronous plugins in the RTS object.
	hr = stylus->AddStylusSyncPlugin(0, handler);
	if (FAILED(hr)) {
		puts("Could not register the stylus plugin");
		reset();
		return false;
	}

	GUID wantedProps[] = {
		GUID_PACKETPROPERTY_GUID_X,
		GUID_PACKETPROPERTY_GUID_Y,
		GUID_PACKETPROPERTY_GUID_NORMAL_PRESSURE,
		GUID_PACKETPROPERTY_GUID_X_TILT_ORIENTATION,
		GUID_PACKETPROPERTY_GUID_Y_TILT_ORIENTATION,
		GUID_PACKETPROPERTY_GUID_PACKET_STATUS,
	};
	stylus->SetDesiredPacketDescription(
		sizeof(wantedProps) / sizeof(*wantedProps), wantedProps);
	stylus->put_Enabled(true);

	handler->gatherContexts();

	puts("Stylus initialization succeeded");
	return true;
}

void StylusManager::reset()
{
	if (stylus) {
		stylus->Release();
		stylus = nullptr;
	}
	if (handler) {
		handler->Release();
		handler = nullptr;
	}
}

/**
 * NetStylus implementation of IStylusSyncPlugin
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
 * This file contains code that receives stylus packets from the RealTimeStylus
 * interface and relays them through the network.
 *
 * It is based on the code from https://backworlds.com/under-pressure/ .
 */

#include "stylus_plugin.h"

#include <ws2tcpip.h>

#include <cstdio>

NetworkStylus::NetworkStylus(IRealTimeStylus *stylus)
{
	mRefCount = 1;

	mStylus = stylus;

	mServer = {};
	mSocket = socket(mServer.sa_family, SOCK_DGRAM, IPPROTO_UDP);
}

NetworkStylus::~NetworkStylus()
{
	if (mPunkFTMarshaller) {
		mPunkFTMarshaller->Release();
	}

	if (mSocket != INVALID_SOCKET) {
		closesocket(mSocket);
		mSocket = INVALID_SOCKET;
	}
}

HRESULT NetworkStylus::createMarshaller()
{
	return CoCreateFreeThreadedMarshaler(mStylus, &mPunkFTMarshaller);
}

void NetworkStylus::gatherContexts()
{
	ULONG nContexts = 0;
	TABLET_CONTEXT_ID *contexts = nullptr;
	HRESULT res = mStylus->GetAllTabletContextIds(&nContexts, &contexts);
	if (FAILED(res) || !nContexts) {
		printf("Cannot get tablets or no tablets found (%lu, %lu)\n", res,
			nContexts);
		return;
	}

	for(ULONG i = 0; i < nContexts; i++) {
		IInkTablet *tablet = nullptr;
		res = mStylus->GetTabletFromTabletContextId(contexts[i], &tablet);
		if (SUCCEEDED(res)) {
			Context ctx;
			float scaleX, scaleY;
			ULONG nProperties;
			PACKET_PROPERTY *properties;
			mStylus->GetPacketDescriptionData(contexts[i], &scaleX, &scaleY,
				&nProperties, &properties);

			for(int j = 0; j < nProperties; j++) {
				if(properties[j].guid == GUID_PACKETPROPERTY_GUID_X) {
					ctx.x = j;
				} else if(properties[j].guid == GUID_PACKETPROPERTY_GUID_Y) {
					ctx.y = j;
				} else if (properties[j].guid
						== GUID_PACKETPROPERTY_GUID_PACKET_STATUS) {
					ctx.status = j;
				} else if(properties[j].guid
						== GUID_PACKETPROPERTY_GUID_NORMAL_PRESSURE) {
					ctx.pressure = j;
					ctx.maxPressure = properties[j].PropertyMetrics.nLogicalMax;
				} else if(properties[j].guid
						== GUID_PACKETPROPERTY_GUID_X_TILT_ORIENTATION) {
					ctx.tiltX = j;
				} else if(properties[j].guid
						== GUID_PACKETPROPERTY_GUID_Y_TILT_ORIENTATION) {
					ctx.tiltY = j;
				}
			}

			if (ctx.x >= 0 && ctx.y >= 0 && ctx.status >= 0) {
				mContexts[contexts[i]] = ctx;
			}

			CoTaskMemFree(properties);
		} else {
			printf("Cannot get tablet information: %lu\n", res);
		}
	}
}

bool NetworkStylus::setServer(const char *addr, uint16_t port)
{
	addrinfo *info = nullptr;
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	if (getaddrinfo(addr, NULL, &hints, &info)) {
		return false;
	}

	mServer = *info->ai_addr;
	sockaddr_in &addrIn = reinterpret_cast<sockaddr_in &>(mServer);
	addrIn.sin_port = htons(port);
	freeaddrinfo(info);

	return true;
}

void NetworkStylus::windowChanged(HWND hWnd)
{
	HDC dc = GetDC(hWnd);

	RECT r;
	GetClientRect(hWnd, &r);
	double width = r.right - r.left;
	double height = r.bottom - r.top;

	double dpmmX = static_cast<double>(GetDeviceCaps(dc, HORZSIZE))
		/ GetDeviceCaps(dc, HORZRES);
	double dpmmY = static_cast<double>(GetDeviceCaps(dc, VERTSIZE))
		/ GetDeviceCaps(dc, VERTRES);
	width *= dpmmX;
	height *= dpmmY;

	if (width < 1 || height < 1) {
		// Minimized, do not update the information
		return;
	}

	mScaleX = (25.4 / dpmmX) / GetDeviceCaps(dc, LOGPIXELSX);
	mScaleY = (25.4 / dpmmY) / GetDeviceCaps(dc, LOGPIXELSY);

	// X and Y are in 10µm, i.e. a mm is 100 units
	mMaxX = static_cast<uint32_t>(width * 100 + 0.5);
	mMaxY = static_cast<uint32_t>(height * 100 + 0.5);
}

void NetworkStylus::sendPackets(const StylusInfo *stylusInfo, ULONG numPackets,
	ULONG totalLength, LONG *packets)
{
	auto it = mContexts.find(stylusInfo->tcid);
	if (it == mContexts.end()) {
		puts("Tablet not found");
		return;
	}

	const uint16_t statusMask = (PacketIsTouching |PacketIsEraser
			| PacketButtonPressed);
	const auto &tablet = it->second;
	ULONG packetSize = totalLength / numPackets;
	for (ULONG i = 0; i < numPackets; i++) {
		Packet p = {};
		strncpy(p.magic, PACKET_MAGIC, sizeof(p.magic));
		p.status = packets[tablet.status] & statusMask;

		p.x = static_cast<uint32_t>(packets[tablet.x] / mScaleX);
		p.maxX = mMaxX;
		p.y = static_cast<uint32_t>(packets[tablet.y] / mScaleY);
		p.maxY = mMaxY;

		if (tablet.pressure >= 0) {
			p.status |=  PacketHasPressure;
			p.pressure = packets[tablet.pressure];
			p.maxPressure = tablet.maxPressure;
		}

		if (tablet.tiltX >= 0) {
			p.status |=  PacketHasTiltX;
			p.tiltX = packets[tablet.tiltX];
		}
		if (tablet.tiltY >= 0) {
			p.status |=  PacketHasTiltY;
			p.tiltY = packets[tablet.tiltY];
		}

		p.seqNumber = mSeqNumber++;

		sendto(mSocket,
			reinterpret_cast<char *>(&p), static_cast<int>(sizeof(p)), 0,
			&mServer, static_cast<int>(sizeof(mServer)));

		packets += packetSize;
	}
}


STDMETHODIMP NetworkStylus::Packets(IRealTimeStylus *pStylus,
	const StylusInfo *pStylusInfo, ULONG nPackets, ULONG nPacketBuf,
	LONG *pPackets, ULONG *nOutPackets, LONG **ppOutPackets) noexcept
{
	sendPackets(pStylusInfo, nPackets, nPacketBuf, pPackets);
	return S_OK;
}

STDMETHODIMP NetworkStylus::InAirPackets(IRealTimeStylus *pStylus,
	const StylusInfo *pStylusInfo, ULONG nPackets, ULONG nPacketBuf,
	LONG *pPackets, ULONG *nOutPackets, LONG **ppOutPackets) noexcept
{
	sendPackets(pStylusInfo, nPackets, nPacketBuf, pPackets);
	return S_OK;
}

STDMETHODIMP NetworkStylus::UpdateMapping(IRealTimeStylus *pStylus) noexcept
{
	gatherContexts();
	return S_OK;
}

STDMETHODIMP NetworkStylus::DataInterest(
	RealTimeStylusDataInterest *pEventInterest) noexcept
{
	*pEventInterest = static_cast<RealTimeStylusDataInterest>(
		RTSDI_Packets | RTSDI_InAirPackets | RTSDI_UpdateMapping);
	return S_OK;
}


STDMETHODIMP NetworkStylus::StylusDown(IRealTimeStylus *, const StylusInfo*,
	ULONG, LONG *_pPackets, LONG **) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::StylusUp(IRealTimeStylus *, const StylusInfo*,
	ULONG, LONG* _pPackets, LONG **) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::RealTimeStylusEnabled(IRealTimeStylus *, ULONG,
	const TABLET_CONTEXT_ID *) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::RealTimeStylusDisabled(IRealTimeStylus *, ULONG,
	const TABLET_CONTEXT_ID *) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::StylusInRange(IRealTimeStylus *, TABLET_CONTEXT_ID,
	STYLUS_ID) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::StylusOutOfRange(IRealTimeStylus *,
	TABLET_CONTEXT_ID, STYLUS_ID) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::StylusButtonUp(IRealTimeStylus *, STYLUS_ID,
	const GUID *, POINT *) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::StylusButtonDown(IRealTimeStylus *, STYLUS_ID,
	const GUID *, POINT *) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::SystemEvent(IRealTimeStylus *, TABLET_CONTEXT_ID,
	STYLUS_ID, SYSTEM_EVENT, SYSTEM_EVENT_DATA) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::TabletAdded(IRealTimeStylus *, IInkTablet *)
	noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::TabletRemoved(IRealTimeStylus *, LONG) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::CustomStylusDataAdded(IRealTimeStylus *,
	const GUID *, ULONG, const BYTE *) noexcept
{
	return S_OK;
}

STDMETHODIMP NetworkStylus::Error(IRealTimeStylus *, IStylusPlugin*,
	RealTimeStylusDataInterest, HRESULT, LONG_PTR*) noexcept
{
	return S_OK;
}


STDMETHODIMP_(ULONG) NetworkStylus::AddRef() noexcept
{
	return ++mRefCount;
}

STDMETHODIMP_(ULONG) NetworkStylus::Release() noexcept
{
	ULONG newVal = --mRefCount;
	if (!newVal) {
		delete this;
	}
	return newVal;
}

STDMETHODIMP NetworkStylus::QueryInterface(REFIID riid, LPVOID *ppvObj) noexcept
{
	if ((riid == IID_IStylusSyncPlugin) || (riid == IID_IUnknown))
	{
		*ppvObj = this;
		AddRef();
		return S_OK;
	}
	else if ((riid == IID_IMarshal) && mPunkFTMarshaller)
	{
		return mPunkFTMarshaller->QueryInterface(riid, ppvObj);
	}

	*ppvObj = nullptr;
	return E_NOINTERFACE;
}

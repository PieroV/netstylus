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

#pragma once

#include <netstylus_packet.h>

#include <winsock2.h>

#include <rtscom.h>
#include <RTSCom_i.c>

#include <atomic>
#include <unordered_map>
#include <cstdint>

/**
 * A synchronous stylus plugin that relays packets thorugh the network
 *
 * It is based on the code from https://backworlds.com/under-pressure/ .
 */
class __declspec(uuid("104112a4-0cff-4201-952b-cdede6b11763")) NetworkStylus
	: public IStylusSyncPlugin
{
public:
	NetworkStylus(IRealTimeStylus *stylus);
	virtual ~NetworkStylus();

	HRESULT createMarshaller();

	/// Gather the available tablets
	void gatherContexts();

	/// Set the destination address for following packets
	bool setServer(const char *addr, uint16_t port);

	/// Tell that a window has changed properties (size and/or resolution)
	void windowChanged(HWND hWnd);

	/// IStylusSyncPlugin
	///@{

	STDMETHOD(Packets)(IRealTimeStylus *pStylus, const StylusInfo *pStylusInfo,
		ULONG nPackets, ULONG nPacketBuf, LONG *pPackets, ULONG *nOutPackets,
		LONG **ppOutPackets);

	STDMETHOD(InAirPackets)(IRealTimeStylus *pStylus,
		const StylusInfo *pStylusInfo, ULONG nPackets, ULONG nPacketBuf,
		LONG *pPackets, ULONG *nOutPackets, LONG **ppOutPackets);

	STDMETHOD(UpdateMapping)(IRealTimeStylus *stylus);

	STDMETHOD(DataInterest)(RealTimeStylusDataInterest *pEventInterest);


	STDMETHOD(StylusDown)(IRealTimeStylus *, const StylusInfo*, ULONG,
		LONG *_pPackets, LONG **);
	STDMETHOD(StylusUp)(IRealTimeStylus *, const StylusInfo*, ULONG,
		LONG* _pPackets, LONG **);
	STDMETHOD(RealTimeStylusEnabled)(IRealTimeStylus *, ULONG,
		const TABLET_CONTEXT_ID *);
	STDMETHOD(RealTimeStylusDisabled)(IRealTimeStylus *, ULONG,
		const TABLET_CONTEXT_ID *);
	STDMETHOD(StylusInRange)(IRealTimeStylus *, TABLET_CONTEXT_ID, STYLUS_ID);
	STDMETHOD(StylusOutOfRange)(IRealTimeStylus *, TABLET_CONTEXT_ID,
		STYLUS_ID);
	STDMETHOD(StylusButtonUp)(IRealTimeStylus *, STYLUS_ID, const GUID *,
		POINT *);
	STDMETHOD(StylusButtonDown)(IRealTimeStylus *, STYLUS_ID, const GUID *,
		POINT *);
	STDMETHOD(SystemEvent)(IRealTimeStylus *, TABLET_CONTEXT_ID, STYLUS_ID,
		SYSTEM_EVENT, SYSTEM_EVENT_DATA);
	STDMETHOD(TabletAdded)(IRealTimeStylus *, IInkTablet *);
	STDMETHOD(TabletRemoved)(IRealTimeStylus *, LONG);
	STDMETHOD(CustomStylusDataAdded)(IRealTimeStylus *, const GUID *, ULONG,
		const BYTE *);
	STDMETHOD(Error)(IRealTimeStylus *, IStylusPlugin*,
		RealTimeStylusDataInterest, HRESULT, LONG_PTR*);

	///@}

	///IUnknown
	///@{
	STDMETHOD_(ULONG,AddRef)();
	STDMETHOD_(ULONG,Release)();
	STDMETHOD(QueryInterface)(REFIID riid, LPVOID *ppvObj);
	///@}

private:

	/**
	 * Information about a tablet
	 *
	 * It contains the offset of these properties, with respect to the beginning
	 * of a stylus packet.
	 * A negative number means that the tablet does not support that feature.
	 */
	struct Context {
		int x = -1;
		int y = -1;
		int pressure = -1;
		int maxPressure = -1;
		int tiltX = -1;
		int tiltY = -1;
		int status = -1;
	};

	/// Send received packets through the network
	void sendPackets(const StylusInfo *stylusInfo, ULONG numPackets,
		ULONG totalLength, LONG *packets);

	/// COM reference count
	std::atomic<ULONG> mRefCount;

	/// Other COM stuff
	IUnknown *mPunkFTMarshaller;

	/// The stylus
	IRealTimeStylus *mStylus;

	/// The maximum X and Y
	uint32_t mMaxX = 0;
	uint32_t mMaxY = 0;

	/// The scale of X and Y, to convert them to real millimeters
	double mScaleX = 1;
	double mScaleY = 1;

	/// The map of available tablets
	std::unordered_map<TABLET_CONTEXT_ID, Context> mContexts;

	/// The address of the server
	sockaddr mServer;

	/// The socket we use to send data
	SOCKET mSocket = INVALID_SOCKET;

	/// The sequence number of the next package we will send
	uint64_t mSeqNumber = 0;
};

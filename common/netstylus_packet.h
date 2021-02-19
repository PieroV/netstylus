/**
 * NetStylus
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
 * This file contains the struct for the packet that is sent by NetStylus over
 * the network.
 */

#pragma once

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

/// The magic string of packets
static const char PACKET_MAGIC[10] = "NetStylus";

/// The structure of a packet we will send through the network
struct Packet {
	char magic[10]; ///< "NetStylus"
	uint16_t status; ///< The status and available data
	uint32_t pressure; ///< The pressure (not always be available)
	uint64_t seqNumber; ///< The packet sequence number
	int32_t maxPressure; ///< The maximum pressore of the stylus
	uint32_t x; ///< x, in mm * 100 (i.e. 10^-5m)
	uint32_t maxX; ///< Width of the window, in mm * 100 (i.e. 10^-5m)
	uint32_t y; ///< y, in mm * 100 (i.e. 10^-5m)
	uint32_t maxY; ///< Height of the window, in mm * 100 (i.e. 10^-5m)
	uint32_t tiltX; ///< Tilt X (not always be available)
	uint32_t tiltY; ///< Tilt Y (not always be available)
};

/// The masks to check if a feature is in the packet
enum PacketFeatures {
	PacketIsTouching = 0x1,
	PacketIsEraser = 0x2,
	PacketButtonPressed = 0x8,
	PacketHasPressure = 0x10,
	PacketHasTiltX = 0x20,
	PacketHasTiltY = 0x40,
};

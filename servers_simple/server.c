/**
 * Simple server for NetStylus
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
 * This file contains a simple server that I used during the early stage of the
 * project development, and may not work.
 */


#include <netstylus_packet.h>

#include <winsock2.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		printf("Could not initialize WinSock: %d\n", WSAGetLastError());
		return 1;
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		printf("Could not create the socket: %d\n", WSAGetLastError());
		return 2;
	}

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(54321);
	if (bind(s, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
		printf("Could not bind the server: %d\n", WSAGetLastError());
		return 3;
	}

	while(1) {
		struct Packet packet;
		struct sockaddr_in peer;
		int peerLen;
		int received = recvfrom(s, (char *)&packet, (int)sizeof(packet), 0,
			(struct sockaddr *)(&peer), &peerLen);
		if (received == SOCKET_ERROR) {
			continue;
		}
		assert(sizeof(peer) == peerLen);

		printf("Received packet from %s: %d\n", inet_ntoa(peer.sin_addr),
			ntohs(peer.sin_port));
		printf("%s %llu %hu %u %u %u %u\n",
			packet.magic, packet.seqNumber, packet.status, packet.x, packet.y,
			packet.maxX, packet.maxY);
	}

	closesocket(s);
	WSACleanup();

	return 0;
}

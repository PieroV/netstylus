/**
 * Evdev server for NetStylus
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
 * This file contains a server to command a Linux computer using NetStylus.
 *
 * To compile: g++ -I../common/ -I/usr/include/libevdev-1.0/ server_evdev.cpp -levdev -o server
 */

#include <netstylus_packet.h>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include <linux/input.h> // input_absinfo

#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <stdexcept>
#include <cassert>
#include <cerrno>
#include <cmath> // M_PI
#include <cstdio>
#include <cstring> // strncmp

class Server {
public:
	~Server();
	int run();

private:
	bool setupSocket();
	void readFirst();
	bool setupDevice();
	void readEvents();

	void packetToEvent(const Packet &p);

	Packet readOne();

	libevdev *mDev = nullptr;
	libevdev_uinput *mUidev = nullptr;

	int mSocket = -1;

	uint64_t mLastSeq = 0;

	uint32_t mMaxX = 16000;
	uint32_t mMaxY = 9000;
	int mMaxPressure = 4096;
};

// For the signal handler, cannot think of anything better :(
static bool canRun = true;

static void handleSigInt(int s);

int main()
{
	struct sigaction action;
	action.sa_handler = handleSigInt;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGINT, &action, NULL);

	Server s;
	return s.run();
}


Server::~Server()
{
	if (mUidev) {
		libevdev_uinput_destroy(mUidev);
		mUidev = nullptr;
	}
	if (mDev) {
		libevdev_free(mDev);
		mDev = nullptr;
	}

	if (mSocket >= 0) {
		close(mSocket);
		mSocket = -1;
	}
}

int Server::run()
{
	if (!setupSocket()) {
		return 1;
	}

	try {
		readFirst();
		if (!canRun) {
			return 0;
		}
		if (!setupDevice()) {
			return 3;
		}
		readEvents();
	} catch (std::exception &e) {
		fprintf(stderr, "Exiting: %s\n", e.what());
		return 2;
	}

	return 0;
}

bool Server::setupSocket()
{
	mSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (mSocket < 0) {
		perror("Could not open a socket");
		return false;
	}

	// Set a timeout, to handle Ctrl-C if needed
	timeval tv = {};
	tv.tv_sec = 0;
	tv.tv_usec = 100000; // 100ms
	setsockopt(mSocket, SOL_SOCKET, SO_RCVTIMEO,
		reinterpret_cast<const char*>(&tv), sizeof(tv));

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(4642);

	int err = bind(mSocket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
	if (err && errno == EADDRINUSE) {
		addr.sin_port = 0;
		err = bind(mSocket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
	}
	if (err) {
		perror("Could not bind the socket");
		return false;
	}

	socklen_t len = sizeof(addr);
	if (getsockname(mSocket, reinterpret_cast<sockaddr *>(&addr), &len)) {
		perror("getsockname failed");
		printf("Socket: %d\n", mSocket);
		return false;
	}
	assert(len == sizeof(addr));
	printf("Listening on port %hu\n", ntohs(addr.sin_port));

	return true;
}

void Server::readFirst()
{
	Packet p = readOne();
	mMaxX = p.maxX;
	mMaxY = p.maxY;
	mMaxPressure = p.maxPressure;
}

bool Server::setupDevice()
{
	mDev = libevdev_new();
	if (!mDev) {
		fputs("libevdev_new returned null\n", stderr);
		return false;
	}

	libevdev_set_name(mDev, "NetStylus");

	int err;
	input_absinfo absValues;

	err = libevdev_enable_event_type(mDev, EV_ABS);
	if (err) {
		printf("Failed to enable abs %d\n", err);
	}

	absValues = {0, 0, static_cast<int>(mMaxX), 0, 0, 100};
	err = libevdev_enable_event_code(mDev, EV_ABS, ABS_X, &absValues);
	if (err) {
		printf("Failed to enable abs X %d\n", err);
	}
	absValues = {0, 0, static_cast<int>(mMaxY), 0, 0, 100};
	err = libevdev_enable_event_code(mDev, EV_ABS, ABS_Y, &absValues);
	if (err) {
		printf("Failed to enable abs Y %d\n", err);
	}

	absValues = {0, 0, mMaxPressure, 0, 0, 1};
	err = libevdev_enable_event_code(mDev, EV_ABS, ABS_PRESSURE, &absValues);
	if (err) {
		printf("Failed to enable abs pressure %d\n", err);
	}

	// 1 unit = 0.01 deg = 0.01 * pi / 180 rad
	absValues = {0, 0, 18000, 0, 0, static_cast<int>(100 * 180 / M_PI)};
	err = libevdev_enable_event_code(mDev, EV_ABS, ABS_TILT_X, &absValues);
	if (err) {
		printf("Failed to enable abs tilt X %d\n", err);
	}
	err = libevdev_enable_event_code(mDev, EV_ABS, ABS_TILT_Y, &absValues);
	if (err) {
		printf("Failed to enable abs tilt Y %d\n", err);
	}

	err = libevdev_enable_event_type(mDev, EV_KEY);
	if (err) {
		printf("Failed to enable key %d\n", err);
	}
	err = libevdev_enable_event_code(mDev, EV_KEY, BTN_TOUCH, nullptr);
	if (err) {
		printf("Failed to enable key touch %d\n", err);
	}
	err = libevdev_enable_event_code(mDev, EV_KEY, BTN_TOOL_PEN, nullptr);
	if (err) {
		printf("Failed to enable key pen %d\n", err);
	}
	err = libevdev_enable_event_code(mDev, EV_KEY, BTN_TOOL_RUBBER, nullptr);
	if (err) {
		printf("Failed to enable key rubber %d\n", err);
	}
	err = libevdev_enable_event_code(mDev, EV_KEY, BTN_STYLUS, nullptr);
	if (err) {
		printf("Failed to enable key stylus %d\n", err);
	}

	err = libevdev_uinput_create_from_device(mDev, LIBEVDEV_UINPUT_OPEN_MANAGED,
		&mUidev);
	if (err) {
		fprintf(stderr, "Could not create the device, error %d\n", err);
		return false;
	}

	return true;
}

void Server::readEvents()
{
	while (canRun) {
		Packet p = readOne();
		if (canRun) {
			packetToEvent(p);
		}
	}
}

static inline void reportError(const Packet &p, const char *descr, int err)
{
	if (err < 0) {
		printf("Packet %lu: failed to write %s (%d)\n",
			p.seqNumber, descr, err);
	}
}

void Server::packetToEvent(const Packet &p)
{
	if (!(p.status & PacketHasPressure)) {
		// Might be a mouse event, discard it
		return;
	}

	int err = 0;

	if (p.maxX != mMaxX) {
		puts("Maximum X changed. "
			"This will not work as expected, accordingly to my experience");
		libevdev_set_abs_maximum(mDev, ABS_X, p.maxX);
		mMaxX = p.maxX;
	}
	if (p.maxY != mMaxY) {
		puts("Maximum Y changed. "
			"This will not work as expected, accordingly to my experience");
		libevdev_set_abs_maximum(mDev, ABS_Y, p.maxY);
		mMaxY = p.maxY;
	}

	err = libevdev_uinput_write_event(mUidev, EV_ABS, ABS_X, p.x);
	reportError(p, "X", err);
	err = libevdev_uinput_write_event(mUidev, EV_ABS, ABS_Y, p.y);
	reportError(p, "Y", err);
	err = libevdev_uinput_write_event(mUidev, EV_ABS, ABS_PRESSURE, p.pressure);
	reportError(p, "pressure", err);

	err = libevdev_uinput_write_event(mUidev, EV_KEY, BTN_TOUCH,
		p.status & PacketIsTouching ? 1 : 0);
	reportError(p, "touch", err);

	if (p.status & PacketIsEraser) {
		err = libevdev_uinput_write_event(mUidev, EV_KEY, BTN_TOOL_RUBBER, 1);
	} else {
		err = libevdev_uinput_write_event(mUidev, EV_KEY, BTN_TOOL_PEN, 1);
	}
	reportError(p, "tool", err);

	err = libevdev_uinput_write_event(mUidev, EV_KEY, BTN_STYLUS,
		p.status & PacketButtonPressed);
	reportError(p, "button", err);

	if (p.status & PacketHasTiltX) {
		err = libevdev_uinput_write_event(mUidev, EV_ABS, ABS_TILT_X, p.tiltX);
		reportError(p, "tilt X", err);
	}

	if (p.status & PacketHasTiltY) {
		err = libevdev_uinput_write_event(mUidev, EV_ABS, ABS_TILT_Y, p.tiltY);
		reportError(p, "tilt Y", err);
	}

	err = libevdev_uinput_write_event(mUidev, EV_SYN, SYN_REPORT, 0);
	if (err < 0) {
		printf("Packet %lu: failed to syn (%d)\n", p.seqNumber, err);
	}
}

Packet Server::readOne()
{
	const uint64_t shouldReset = 100;

	Packet p;
	ssize_t bytes;
	do {
		bytes = read(mSocket, reinterpret_cast<char *>(&p), sizeof(p));
		if (bytes <= 0) {
			continue;
		}
		p.magic[sizeof(p.magic) - 1] = 0;
		if (strncmp(p.magic, PACKET_MAGIC, sizeof(p.magic))) {
			continue;
		}

		if (p.seqNumber <= mLastSeq && (mLastSeq - p.seqNumber) < shouldReset) {
			continue;
		}

		mLastSeq = p.seqNumber;
		break;
	} while ((bytes >= 0 || errno == EAGAIN) && canRun);

	if (bytes < 0 && canRun) {
		std::string msg = "Error while reading the packet: ";
		msg += strerror(errno);
		throw std::runtime_error(msg);
	}
	return p;
}

void handleSigInt(int s)
{
	canRun = false;
}

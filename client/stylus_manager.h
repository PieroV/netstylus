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

#pragma once

#include "stylus_plugin.h"

/**
 * This class initializes the real time stylus support and registers the
 * NetStylus plugin.
 *
 * It is based on the code from https://backworlds.com/under-pressure/ .
 */
class StylusManager {
public:
	~StylusManager();

	bool setup(HWND hWnd);
	void reset();

	IRealTimeStylus *stylus = nullptr;
	NetworkStylus *handler = nullptr;
};

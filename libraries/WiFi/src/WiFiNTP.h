/*
    WiFiNTP.h - Simple NTP wrapper for LWIP
    Copyright (c) 2022 Earle F. Philhower, III.  All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include <Arduino.h>
#include <lwip/apps/sntp.h>

class NTPClass {
public:
    NTPClass() { }

    ~NTPClass() {
        sntp_stop();
    }

    void begin(IPAddress server, int timeout = 3600) {
        (void) timeout;
        sntp_stop();
        if (server.isSet()) {
            sntp_setserver(0, server);
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_init();
        }
    }

    void begin(const char *server, int timeout = 3600) {
        IPAddress addr;
        if (WiFi.hostByName(server, addr)) {
            begin(addr, timeout);
        }
    }
};

extern NTPClass NTP;

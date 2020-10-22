/*
 * Copyright (c) 2020 Particle Industries, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// FSLogHandler Example: A log handler that writes logs to the onboard filesystem on Gen 3 and Tracker
// Author:  Dan Kouba <dan.kouba@particle.io>
// Date:    October 2020

#include "Particle.h"

#include "tracker_config.h"
#include "tracker.h"

#include "FSLogHandler.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

PRODUCT_ID(TRACKER_PRODUCT_ID);
PRODUCT_VERSION(TRACKER_PRODUCT_VERSION);

// AT + modem logs routed to the serial terminal
SerialLogHandler logHandler(115200, LOG_LEVEL_INFO, {
    { "ncp.at",         LOG_LEVEL_INFO },
    { "net.ppp.client", LOG_LEVEL_INFO },
});

// GPS Trace logs routed to the filesystem
// If you send these logs out to the cloud, DO NOT include modem and AT logs here, as it will recurse
FSLogHandler fslog("test", false, LOG_LEVEL_NONE, {
    { "app.gps.nmea", LOG_LEVEL_TRACE },
    { "app.gps.ubx",  LOG_LEVEL_TRACE },
});

void setup()
{
    fslog.clearLogs();    // Start fresh with a new logfile

    // Wait for serial connectivity so we can watch debug logs (optional)
    // while (!Serial.isConnected()) {
    //     Particle.process();
    //     delay(500);
    // }

    fslog.enable();     // Delayed enable
    Tracker::instance().init();
    Particle.connect();
}

void loop()
{
    // Periodically dump out new log data to the serial terminal
    static unsigned int now = System.uptime();
    if (System.uptime() - now > 30) {
        now = System.uptime();
        Log.info("Dumping logfile: %s (size: %s)", fslog.getPath().c_str(), bytesToFilesize(fslog.getLogSize()));

        // Dump using printf
        Serial.println("\nbegin new data --->");
        fslog.dump(Serial, false);
        Serial.println("---> end\n");
    }

    Tracker::instance().loop();
    fslog.loop();
}

const char* bytesToFilesize(long bytes) {
    String s;
    if (bytes > 1000000) {
        s.concat(String::format("%0.2f MB", (float)bytes / 1000000.0));
    } else if (bytes > 1000) {
        s.concat(String::format("%0.2f kB", (float)bytes / 1000.0));
    } else {
        s.concat(String::format("%li B", bytes));
    }
    return s.c_str();
}
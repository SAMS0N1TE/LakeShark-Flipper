#pragma once

/* A known transmitter for calibrating the board's direction finder.

   The internal CC1101 sends OOK carrier bursts, 250 ms on, 50 ms off. The
   board reads its receiver's level about every 110 ms and keeps the
   strongest level per 5 degrees of heading, so a beacon that is on most of
   the time lands in nearly every read, and the gaps cost nothing.

   Two bands, both at about -10 dBm:
   - 915.000 MHz, the default. In the US the 902-928 MHz band allows a
     continuous low-power transmitter (47 CFR 15.249: 50 mV/m at 3 m, about
     -1 dBm radiated), and the board's own LoRa radio hears it on the antenna
     it already has.
   - 433.920 MHz, for regions without 915. Check the local rules for a
     transmitter that is on most of the time.
   It stops by itself after three minutes, and whenever its screen is left,
   so it never transmits unnoticed. */

#include <stdbool.h>
#include <stdint.h>

#define LS_BEACON_ON_MS     250
#define LS_BEACON_OFF_MS    50
#define LS_BEACON_RUN_S     180

typedef enum {
    LsBeaconBand915,
    LsBeaconBand433,
    LsBeaconBandCount,
} LsBeaconBand;

typedef struct LsBeacon LsBeacon;

LsBeacon* ls_beacon_alloc(void);
void ls_beacon_free(LsBeacon* b);

/* Only while stopped; a running beacon keeps its band. */
void ls_beacon_set_band(LsBeacon* b, LsBeaconBand band);
LsBeaconBand ls_beacon_band(const LsBeacon* b);
uint32_t ls_beacon_hz(const LsBeacon* b);
const char* ls_beacon_band_name(const LsBeacon* b);

/* False when the radio or the region refuses; ls_beacon_error says why. */
bool ls_beacon_start(LsBeacon* b);
void ls_beacon_stop(LsBeacon* b);
/* Call often: ends the run when its time is up. */
void ls_beacon_tick(LsBeacon* b);

bool ls_beacon_running(const LsBeacon* b);
uint32_t ls_beacon_seconds_left(const LsBeacon* b);
uint32_t ls_beacon_bursts(const LsBeacon* b);
const char* ls_beacon_error(const LsBeacon* b);

# Recording download verification

The saved-file downloader previously treated cached DONE telemetry as completion
of a new REC LOAD command. Loading a 250-edge file after a 200-edge file could
therefore save only its first 200 edges.

The downloader now waits for a fresh `+OK load=<selected index> ph=3 ...`
acknowledgment and uses its edge count, frequency, and duration. An early empty
chunk, excess edges, or a duration mismatch aborts the save. Firmware without
the correlated acknowledgment times out on saved-file downloads; it must be
updated with the matching REC LOAD protocol change.

Host parser and stale-reply regression: `python tests/test_rec_load_ack.py`
(requires GCC). The app builds with uFBT API 87.1.

Hardware acceptance remains pending for this app build. Alternate the known
200-edge and 250-edge files in both directions, read the resulting Flipper SD
files, and compare every edge, total duration, and frequency against the LCD
source. Leave the LCD running after each transfer and confirm unchanged uptime.

The LCD BLE command stack overflow has a separate firmware fix. A delayed LCD
watchdog reset also reproduced during testing and remains unresolved. Do not
publish a stable release claiming the complete download issue is resolved yet.

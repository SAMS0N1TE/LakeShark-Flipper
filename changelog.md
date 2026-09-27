v2.8:
Settings, Levels has a Voice row that sets the radio's speech level, as a share of Vol, in steps of 5. It shows the radio's own level once the radio reports it; that needs LakeShark 2.5.0.
Opening the app is reliable after a long session. The ADS-B map now loads only on its own page and gives its memory back when you leave it, and every large allocation checks that the heap can spare it first.
Large offline maps draw over Bluetooth. An archive whose tiles are all listed in its root directory is read from the SD card in place instead of indexed in memory, and the tile buffer takes what the heap can spare.

v2.7:
Added DF Beacon to the launcher: a low-power (about -10 dBm) OOK transmitter on 915 MHz, or 433.92 MHz with LEFT and RIGHT, 250 ms on and 50 ms off, for calibrating the radio's COMPASS FIND against a known bearing. It needs no link to the radio. OK starts and stops it, leaving the screen stops it, and it stops by itself after three minutes. It says so when the region or the radio refuses to transmit.

v2.6:
Saved-file downloads work again. The REC LOAD acknowledgement was parsed with %llu, which the Flipper's scanf does not support, so every download from the FILES page timed out as Load failed. Files of 12 to 400 edges now arrive with every edge, the duration and the frequency matching the radio's copy.
A SYSTEM page in P25 shows the loaded profile, GPS site following, Phase II follow with its grant count, and how many valid-looking frames the radio held back as noise. OK on PH2 FOLLOW switches it, OK on a profile loads it from the radio's card, and a long press of OK re-reads the card. Needs LakeShark firmware with the PSYS and PROF commands; an older radio says so.
Downloaded files no longer repeat the frequency when the radio's name already ends in it.
The SIGNAL page traces what matters for each receiver: audio for FM and POCSAG, message rate, tracked aircraft, CRC failures and peak magnitude for ADS-B, which now has the page too.

v2.4:
POCSAG can be tuned. It never had the VFO page, so there was no way to enter a frequency for it; it now has the same frequency, step, volume, gain, squelch and mute controls the other modes have, including the numeric entry keypad on a long press of OK.
The REC app gained a signal page: a scrolling magnitude trace with the detector threshold drawn across it, so a frequency can be found by eye. Up and down retune while watching it, OK arms and disarms without leaving the page, and a long press of OK clears the trace. A finished capture reports why it ended - the transmission stopping, or hitting the span or edge limit - with the shortest and longest mark it saw and an estimated baud rate.
The record page can now configure the whole detector rather than just threshold and gap: tuner bandwidth, minimum pulse, maximum span and minimum edges, with a long press of OK returning bandwidth to auto. Added a 432.80 preset.

v2.3:
Added the REC app: an OOK recorder driven from the Flipper. The record page arms and disarms the radio and adjusts frequency, gain, detector threshold and the silence gap that ends a capture, with a live magnitude and threshold readout for tuning. The capture page shows the pulse train and writes it to the Flipper as a SubGHz RAW .sub file in subghz/lakeshark, ready to replay from the SubGHz app.

v2.1:
Added squelch to the FM page with an open or muted readout, adjusted with up and down while the VFO is not focused for tuning. Added SDR health notifications so the head alerts when the receiver drops or goes silent, and again when it comes back. Slowed the receive flash and made it hold as a clean screen inversion instead of a single frame.

v2.0:
Paged interface with a launcher for P25, FM, POCSAG and ADS-B. Back goes up a level and only exits from the launcher. Settings split into levels, link, device, display and about. Device page can reboot the radio, reset the coprocessor, restart or power cycle the SDR, and turn its Bluetooth off. Added memories and presets with a per app memory page. Added the signal scope, POCSAG log and ADS-B traffic and aircraft pages. Live uptime and free memory readouts.

v1.0:
Initial release. UART link to the radio on pins 13 and 14. Frequency, volume and gain control. P25 talkgroup and NAC display with an S meter.

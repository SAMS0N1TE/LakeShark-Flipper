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

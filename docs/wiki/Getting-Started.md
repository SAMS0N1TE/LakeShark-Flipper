# Getting started

## Install the app

Download the matching Flipper app from the [LCD release](https://github.com/SAMS0N1TE/LakeShark/releases/tag/v1.0.4). With qFlipper, copy `lakeshark_p25.fap` into the SD card's `apps/GPIO` folder, replacing the older copy. Open **Applications → GPIO → LakeShark SDR**.

The ESP32-P4 needs its own LakeShark firmware, an RTL-SDR and independent power. Do not power it from the Flipper's 5 V pin.

## Bluetooth

1. In the Flipper system settings, turn **Bluetooth on** and **Expansion Modules off**.
2. Open LakeShark, then **Settings → LINK**.
3. Select **Transport**, and press OK to choose **Bluetooth**.
4. Enable the LakeShark radio's Bluetooth link with `ble on` from its USB console.
5. Wait for **linked** on the Flipper.

The Flipper advertises; the P4 connects to it. No pairing code is needed. **Advertising** means the head is waiting; **connected** means the transport is attached but telemetry has not yet been decoded; **linked** means telemetry is arriving. A filled header pip also indicates incoming telemetry.

## UART

Connect Flipper **pin 13 TX → radio RX**, **pin 14 RX ← radio TX**, and **pin 11 GND → radio GND**. Use the link UART pins documented for your particular board, not an arbitrary UART header. Select UART in the LINK settings. The link uses **115200 baud, 8N1**.

## Navigation

Use Up/Down to choose rows, OK to activate or edit, and Left/Right to change pages when not editing. Back closes an edit or detail view before returning to the launcher. Some actions use a held OK; check the relevant page guide rather than assuming every OK action behaves the same way.

[Return to the guide](Home.md)

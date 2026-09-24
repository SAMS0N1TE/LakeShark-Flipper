/*LS-840  Alerts, ported from ZeroMesh.

   A receiver in a bag is a receiver you cannot watch, so it has to tell you
   when something happened. ZeroMesh already had the whole stack - an RTTTL
   player, nineteen tones, and independent LED / vibration / sound toggles -
   written for this hardware. Both projects are GPL-3.0 and the same author.

   Takes an LsAlertCtx rather than the app, for the same reason ls_map does:
   the coupling is five fields, and the P4 may want this later. */
#pragma once

#include "ls_alert_types.h"

void ringtones_scan(LsAlertCtx* app);
uint16_t ringtone_total(const LsAlertCtx* app);
void ringtone_label(const LsAlertCtx* app, uint16_t index, char* out, size_t cap);
int16_t ringtone_index_of(const LsAlertCtx* app, const char* filename);
bool rtttl_play_custom(const LsAlertCtx* app, uint16_t index);

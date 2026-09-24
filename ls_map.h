/*LS-836  The map, from ZeroMesh.

   Ported rather than reinvented: the tiling, the pmtiles reader, the mono
   carto style, focus/track and the pan-zoom input model are all solved there
   and were written for this exact screen. Both projects are GPL-3.0 and the
   same author, so this is a move, not a borrowing.

   The whole coupling to the host app turned out to be six fields per marker -
   node_id, has_position, has_name, short_name, latitude_i, longitude_i - so
   LsMapPoint keeps those names and aircraft are shaped into them. That is why
   an aircraft has a "node_id" here: it is the ICAO, and renaming it would have
   meant touching a thousand lines of working renderer to no benefit.

   Tiles are read from ZeroMesh's own directory on purpose. A pmtiles archive
   is tens of megabytes and nobody should download it twice to run two apps by
   the same author on the same SD card. */
#pragma once

#include "ls_map_types.h"

#define MAP_W       128
#define MAP_H       64
#define MAP_MIN_Z   1
#define MAP_MAX_Z   14

bool map_alloc(LsMapCtx* app);

void map_free(LsMapCtx* app);

void map_tick(LsMapCtx* app);

bool map_focus_node(LsMapCtx* app, uint32_t node_id);

bool map_view_center(LsMapCtx* app, int32_t* lat_i, int32_t* lon_i);

void render_map(Canvas* canvas, LsMapCtx* app);

bool map_wants_key(LsMapCtx* app, InputKey key);
void input_map(InputEvent* e, LsMapCtx* app);

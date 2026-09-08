#pragma once

/*LS-836  What the map needs from whoever is hosting it, and nothing else.

   The obvious port would have bound the renderer to LsApp the way ZeroMesh
   binds it to ZeroMeshApp. That does not work here - LsApp is declared inside
   lakeshark_p25.c, not a header - and it would not have been right anyway: the
   same renderer is wanted on the P4 later, where there is no LsApp at all.

   So the host passes a context. The entire coupling turned out to be a mutex,
   the opaque map state, a list of markers and a redraw flag. */

#include <furi.h>
#include <gui/gui.h>

/* One thing to draw on the map.

   Field names are ZeroMesh's on purpose. The renderer reads node_id,
   has_position, has_name, short_name, latitude_i and longitude_i, and keeping
   those names meant porting a thousand lines of working, screen-tuned
   rendering without touching it. For LakeShark an aircraft's node_id is its
   ICAO address and short_name is its callsign. */
typedef struct {
    uint32_t node_id;
    int32_t latitude_i;  /* 1e-7 degrees, as Meshtastic and the renderer use */
    int32_t longitude_i;
    bool has_position;
    bool has_name;
    char short_name[12];
} LsMapPoint;

/* Matches LS_AC_MAX; the map cannot show more than the link carries. */
#define LS_MAP_MAX_PTS 16

typedef struct {
    LsMapPoint pts[LS_MAP_MAX_PTS];
    uint8_t count;
} LsMapRoster;

/* Opaque: allocated by map_alloc, and only ls_map.c knows the shape. */
typedef struct MapState MapState;

typedef struct {
    FuriMutex* lock;
    MapState* map;
    LsMapRoster map_roster;
    bool need_render;
} LsMapCtx;

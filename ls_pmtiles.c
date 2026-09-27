#include "ls_pmtiles.h"

#include <furi.h>
#include <core/memmgr_heap.h>
#include <storage/storage.h>
#include <string.h>

#define TAG "zeromesh_pmtiles"

bool ls_heap_fits(size_t bytes) {
    return memmgr_heap_get_max_free_block() >= bytes + LS_HEAP_SPARE;
}

#define PM_HEADER_LEN     127
#define PM_COMPRESSION_NONE 1
#define PM_TILETYPE_MVT     1

#define PM_MAX_ROOT_DIR (48 * 1024)

/* With leaf directories the root holds one entry per leaf, so only a single
   leaf is ever resident. That is what keeps a statewide archive open in a
   heap this size: cost stops scaling with tile count. */
#define PM_MAX_LEAF_BYTES   (4 * 1024)
#define PM_MAX_LEAF_ENTRIES 256

#define PM_STEP 64

/* Where entry k*PM_STEP starts in each of the four columns, and what the
   entry before it left behind for the ones that are delta coded. */
typedef struct {
    uint64_t first_id;
    uint64_t id_before;
    uint32_t ids_pos, runs_pos, len_pos, off_pos;
    uint32_t prev_off, prev_len;
} PmCheck;

struct PmTiles {
    Storage* storage;
    File* file;

    uint64_t data_offset;
    uint32_t count;

    uint64_t* ids;
    uint32_t* offsets;
    uint32_t* lengths;
    uint8_t* runs;
    uint32_t max_len;

    uint64_t leaf_offset;
    uint64_t leaf_length;
    uint8_t* leaf_buf;
    uint32_t leaf_cached_off;
    uint32_t leaf_cached_len;
    uint32_t* len_scratch;
    uint32_t tile_total;

    uint8_t min_zoom;
    uint8_t max_zoom;

    /* A root directory with no leaves lists every tile, and its index
       costs 17 bytes a tile - more than the heap has with the app open. It
       is read from the card instead, from checkpoints every PM_STEP
       entries, so only a checkpoint's worth is decoded per lookup. */
    bool stream;
    uint64_t root_off;
    uint32_t root_len;
    PmCheck* checks;
    uint32_t n_checks;
};

static uint64_t rd_varint(const uint8_t* b, size_t len, size_t* p) {
    uint64_t r = 0;
    int s = 0;
    while(*p < len && s < 64) {
        uint8_t c = b[(*p)++];
        r |= (uint64_t)(c & 0x7f) << s;
        if(!(c & 0x80)) break;
        s += 7;
    }
    return r;
}

static uint64_t zxy_to_tileid(uint8_t z, uint32_t x, uint32_t y) {

    uint64_t acc = (((uint64_t)1 << (z * 2)) - 1) / 3;
    uint64_t d = 0;
    uint32_t tx = x, ty = y;
    for(uint32_t s = (uint32_t)1 << (z ? z - 1 : 0); z && s > 0; s >>= 1) {
        uint32_t rx = (tx & s) ? 1 : 0;
        uint32_t ry = (ty & s) ? 1 : 0;
        d += (uint64_t)s * s * ((3 * rx) ^ ry);

        if(ry == 0) {
            if(rx == 1) {
                tx = s - 1 - tx;
                ty = s - 1 - ty;
            }
            uint32_t t = tx;
            tx = ty;
            ty = t;
        }
    }
    return acc + d;
}

static int64_t dir_upper(const uint64_t* ids, uint32_t count, uint64_t want) {
    uint32_t lo = 0, hi = count;
    int64_t best = -1;
    while(lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if(ids[mid] <= want) {
            best = (int64_t)mid;
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return best;
}

static bool read_at(File* f, uint64_t off, void* dst, size_t len) {
    if(!storage_file_seek(f, (uint32_t)off, true)) return false;
    uint8_t* p = dst;
    size_t done = 0;
    while(done < len) {
        size_t want = len - done;
        if(want > 0x8000) want = 0x8000;
        uint16_t got = storage_file_read(f, p + done, (uint16_t)want);
        if(got == 0) return false;
        done += got;
    }
    return true;
}

typedef struct {
    File* f;
    uint64_t base;
    uint32_t len;
    uint32_t pos;
    uint32_t buf_start;
    uint16_t buf_fill;
    bool bad;
    uint8_t buf[128];
} PmReader;

static void rdr_init(PmReader* r, File* f, uint64_t base, uint32_t len, uint32_t pos) {
    r->f = f;
    r->base = base;
    r->len = len;
    r->pos = pos;
    r->buf_start = 0;
    r->buf_fill = 0;
    r->bad = false;
}

static uint8_t rdr_byte(PmReader* r) {
    if(r->pos >= r->len) {
        r->bad = true;
        return 0;
    }
    if(r->pos < r->buf_start || r->pos >= r->buf_start + r->buf_fill) {
        uint32_t n = r->len - r->pos;
        if(n > sizeof(r->buf)) n = sizeof(r->buf);
        if(!read_at(r->f, r->base + r->pos, r->buf, n)) {
            r->bad = true;
            return 0;
        }
        r->buf_start = r->pos;
        r->buf_fill = (uint16_t)n;
    }
    return r->buf[r->pos++ - r->buf_start];
}

static uint64_t rdr_varint(PmReader* r) {
    uint64_t v = 0;
    for(int s = 0; s < 64 && !r->bad; s += 7) {
        uint8_t c = rdr_byte(r);
        v |= (uint64_t)(c & 0x7f) << s;
        if(!(c & 0x80)) break;
    }
    return v;
}

/* One pass over the directory: checkpoints, and the largest tile. */
static bool stream_index(PmTiles* p) {
    const uint32_t n = p->count;
    p->n_checks = (n + PM_STEP - 1) / PM_STEP;
    if(!ls_heap_fits(sizeof(PmCheck) * p->n_checks)) {
        FURI_LOG_E(TAG, "no room for %lu checkpoints", (unsigned long)p->n_checks);
        return false;
    }
    p->checks = malloc(sizeof(PmCheck) * p->n_checks);
    if(!p->checks) return false;
    memset(p->checks, 0, sizeof(PmCheck) * p->n_checks);

    PmReader r;
    rdr_init(&r, p->file, p->root_off, p->root_len, 0);
    rdr_varint(&r);

    uint64_t last = 0;
    for(uint32_t i = 0; i < n; i++) {
        PmCheck* c = (i % PM_STEP) ? NULL : &p->checks[i / PM_STEP];
        if(c) {
            c->ids_pos = r.pos;
            c->id_before = last;
        }
        last += rdr_varint(&r);
        if(c) c->first_id = last;
    }
    for(uint32_t i = 0; i < n; i++) {
        if(!(i % PM_STEP)) p->checks[i / PM_STEP].runs_pos = r.pos;
        rdr_varint(&r);
    }
    uint32_t prev_len = 0;
    for(uint32_t i = 0; i < n; i++) {
        if(!(i % PM_STEP)) {
            p->checks[i / PM_STEP].len_pos = r.pos;
            p->checks[i / PM_STEP].prev_len = prev_len;
        }
        prev_len = (uint32_t)rdr_varint(&r);
        if(prev_len > p->max_len) p->max_len = prev_len;
    }
    /* Offsets need the lengths beside them: a zero means "just after the
       previous tile". A second reader walks the lengths in step. */
    PmReader lr;
    rdr_init(&lr, p->file, p->root_off, p->root_len, p->checks[0].len_pos);
    uint32_t prev_off = 0;
    prev_len = 0;
    for(uint32_t i = 0; i < n; i++) {
        if(!(i % PM_STEP)) {
            p->checks[i / PM_STEP].off_pos = r.pos;
            p->checks[i / PM_STEP].prev_off = prev_off;
        }
        const uint64_t v = rdr_varint(&r);
        const uint32_t cur = (v == 0) ? (i ? prev_off + prev_len : 0) : (uint32_t)(v - 1);
        prev_len = (uint32_t)rdr_varint(&lr);
        prev_off = cur;
    }
    if(r.bad || lr.bad) {
        FURI_LOG_E(TAG, "root directory shorter than its entry count");
        return false;
    }
    return true;
}

static bool stream_find(PmTiles* p, uint64_t want, uint32_t* out_off, uint32_t* out_len) {
    uint32_t lo = 0, hi = p->n_checks;
    int64_t k = -1;
    while(lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if(p->checks[mid].first_id <= want) {
            k = mid;
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if(k < 0) return false;
    const PmCheck* c = &p->checks[k];
    const uint32_t first = (uint32_t)k * PM_STEP;
    uint32_t end = first + PM_STEP;
    if(end > p->count) end = p->count;

    PmReader r;
    rdr_init(&r, p->file, p->root_off, p->root_len, c->ids_pos);
    uint64_t last = c->id_before, hit_id = 0;
    int64_t hit = -1;
    for(uint32_t i = first; i < end; i++) {
        last += rdr_varint(&r);
        if(last > want) break;
        hit = i;
        hit_id = last;
    }
    if(hit < 0 || r.bad) return false;

    rdr_init(&r, p->file, p->root_off, p->root_len, c->runs_pos);
    uint64_t run = 0;
    for(uint32_t i = first; i <= (uint32_t)hit; i++)
        run = rdr_varint(&r);
    if(run == 0 || want >= hit_id + run) return false;

    PmReader lr;
    rdr_init(&r, p->file, p->root_off, p->root_len, c->off_pos);
    rdr_init(&lr, p->file, p->root_off, p->root_len, c->len_pos);
    uint32_t prev_off = c->prev_off, prev_len = c->prev_len;
    for(uint32_t i = first; i <= (uint32_t)hit; i++) {
        const uint64_t v = rdr_varint(&r);
        const uint32_t cur = (v == 0) ? (i ? prev_off + prev_len : 0) : (uint32_t)(v - 1);
        prev_len = (uint32_t)rdr_varint(&lr);
        prev_off = cur;
    }
    if(r.bad || lr.bad) return false;
    *out_off = prev_off;
    *out_len = prev_len;
    return true;
}

static bool leaf_load(PmTiles* p, uint32_t off, uint32_t len) {
    if(len && p->leaf_cached_len == len && p->leaf_cached_off == off) return true;
    if(len == 0 || len > PM_MAX_LEAF_BYTES) {
        FURI_LOG_E(
            TAG,
            "leaf dir %lu b over %u, repack with a smaller --leaf-size",
            (unsigned long)len,
            (unsigned)PM_MAX_LEAF_BYTES);
        return false;
    }
    if(!read_at(p->file, p->leaf_offset + off, p->leaf_buf, len)) return false;
    p->leaf_cached_off = off;
    p->leaf_cached_len = len;
    return true;
}

/* Directories store all ids, then all run lengths, then all lengths, then
   all offsets, so every section has to be walked to reach one entry. An
   offset of 0 back-references the end of the previous tile, which is why
   the lengths are kept. */
static bool leaf_find(PmTiles* p, uint64_t want, uint32_t* out_off, uint32_t* out_len) {
    const uint8_t* b = p->leaf_buf;
    size_t len = p->leaf_cached_len;
    size_t q = 0;

    uint64_t n = rd_varint(b, len, &q);
    if(n == 0 || n > PM_MAX_LEAF_ENTRIES) return false;

    uint64_t last = 0, hit_id = 0;
    int64_t hit = -1;
    for(uint64_t i = 0; i < n; i++) {
        last += rd_varint(b, len, &q);
        if(last <= want) {
            hit = (int64_t)i;
            hit_id = last;
        }
    }
    if(hit < 0) return false;

    uint64_t run = 0;
    for(uint64_t i = 0; i < n; i++) {
        uint64_t r = rd_varint(b, len, &q);
        if((int64_t)i == hit) run = r;
    }
    if(run == 0 || want >= hit_id + run) return false;

    for(uint64_t i = 0; i < n; i++)
        p->len_scratch[i] = (uint32_t)rd_varint(b, len, &q);

    uint32_t prev = 0, found = 0;
    for(uint64_t i = 0; i < n; i++) {
        uint64_t v = rd_varint(b, len, &q);
        uint32_t cur = (v == 0) ? (i ? prev + p->len_scratch[i - 1] : 0) : (uint32_t)(v - 1);
        if((int64_t)i == hit) found = cur;
        prev = cur;
    }

    *out_off = found;
    *out_len = p->len_scratch[hit];
    return true;
}

/* The header records no maximum tile size, and with leaves the root holds
   directory sizes, so the caller cannot size its tile buffer without this. */
static bool scan_leaf_tile_sizes(PmTiles* p) {
    p->tile_total = 0;
    for(uint32_t i = 0; i < p->count; i++) {
        if(p->runs[i] != 0) continue;
        if(!leaf_load(p, p->offsets[i], p->lengths[i])) return false;

        const uint8_t* b = p->leaf_buf;
        size_t len = p->leaf_cached_len;
        size_t q = 0;
        uint64_t n = rd_varint(b, len, &q);
        if(n == 0 || n > PM_MAX_LEAF_ENTRIES) {
            FURI_LOG_E(
                TAG,
                "leaf holds %lu entries, max %u",
                (unsigned long)n,
                (unsigned)PM_MAX_LEAF_ENTRIES);
            return false;
        }
        p->tile_total += (uint32_t)n;
        for(uint64_t k = 0; k < n; k++)
            rd_varint(b, len, &q);
        for(uint64_t k = 0; k < n; k++)
            rd_varint(b, len, &q);
        for(uint64_t k = 0; k < n; k++) {
            uint32_t tl = (uint32_t)rd_varint(b, len, &q);
            if(tl > p->max_len) p->max_len = tl;
        }
    }
    return true;
}

PmTiles* pmtiles_open(const char* path) {
    PmTiles* p = malloc(sizeof(PmTiles));
    if(!p) return NULL;
    memset(p, 0, sizeof(PmTiles));

    p->storage = furi_record_open(RECORD_STORAGE);
    p->file = storage_file_alloc(p->storage);

    uint8_t* dir = NULL;

    if(!storage_file_open(p->file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_W(TAG, "cannot open %s", path);
        goto fail;
    }

    uint8_t h[PM_HEADER_LEN];
    if(!read_at(p->file, 0, h, sizeof(h))) {
        FURI_LOG_E(TAG, "short header");
        goto fail;
    }
    if(memcmp(h, "PMTiles", 7) != 0 || h[7] != 3) {
        FURI_LOG_E(TAG, "not a PMTiles v3 archive");
        goto fail;
    }
    if(h[97] != PM_COMPRESSION_NONE || h[98] != PM_COMPRESSION_NONE) {
        FURI_LOG_E(
            TAG, "compressed archive (internal=%u tile=%u); rebuild with none", h[97], h[98]);
        goto fail;
    }
    if(h[99] != PM_TILETYPE_MVT) {
        FURI_LOG_E(TAG, "tile type %u is not MVT", h[99]);
        goto fail;
    }

    uint64_t root_off, root_len, leaf_len;
    memcpy(&root_off, h + 8, 8);
    memcpy(&root_len, h + 16, 8);
    memcpy(&leaf_len, h + 48, 8);
    memcpy(&p->leaf_offset, h + 40, 8);
    memcpy(&p->data_offset, h + 56, 8);
    p->leaf_length = leaf_len;
    p->min_zoom = h[100];
    p->max_zoom = h[101];

    if(leaf_len != 0) {
        if(!ls_heap_fits(PM_MAX_LEAF_BYTES + sizeof(uint32_t) * PM_MAX_LEAF_ENTRIES)) {
            FURI_LOG_E(TAG, "no room for leaf directory buffers");
            goto fail;
        }
        p->leaf_buf = malloc(PM_MAX_LEAF_BYTES);
        p->len_scratch = malloc(sizeof(uint32_t) * PM_MAX_LEAF_ENTRIES);
        if(!p->leaf_buf || !p->len_scratch) {
            FURI_LOG_E(TAG, "out of memory for leaf directory buffers");
            goto fail;
        }
    }
    if(root_len == 0 || root_len > PM_MAX_ROOT_DIR) {
        FURI_LOG_E(TAG, "root dir %lu bytes out of range", (unsigned long)root_len);
        goto fail;
    }

    p->root_off = root_off;
    p->root_len = (uint32_t)root_len;

    /* Leafless and too big to index in this heap: read it in place. */
    if(leaf_len == 0) {
        uint8_t head[10];
        const size_t hl = root_len < sizeof(head) ? (size_t)root_len : sizeof(head);
        size_t hq = 0;
        if(!read_at(p->file, root_off, head, hl)) goto fail;
        const uint64_t entries = rd_varint(head, hl, &hq);
        const size_t index_bytes = (sizeof(uint64_t) + 2 * sizeof(uint32_t) + 1) * (size_t)entries;
        if(entries && entries <= 100000 &&
           !ls_heap_fits((size_t)root_len + index_bytes)) {
            p->count = (uint32_t)entries;
            p->stream = true;
            if(!stream_index(p)) goto fail;
            p->tile_total = p->count;
            FURI_LOG_I(
                TAG,
                "streaming %s: %lu tiles z%u-%u, max tile %lu b, %lu checkpoints",
                path,
                (unsigned long)p->tile_total,
                p->min_zoom,
                p->max_zoom,
                (unsigned long)p->max_len,
                (unsigned long)p->n_checks);
            return p;
        }
    }

    if(!ls_heap_fits((size_t)root_len)) {
        FURI_LOG_E(TAG, "no room for a %lu byte root dir", (unsigned long)root_len);
        goto fail;
    }
    dir = malloc((size_t)root_len);
    if(!dir || !read_at(p->file, root_off, dir, (size_t)root_len)) {
        FURI_LOG_E(TAG, "cannot read root dir");
        goto fail;
    }

    size_t q = 0;
    uint64_t n = rd_varint(dir, (size_t)root_len, &q);
    if(n == 0 || n > 100000) {
        FURI_LOG_E(TAG, "entry count %lu implausible", (unsigned long)n);
        goto fail;
    }
    p->count = (uint32_t)n;

    /* The raw directory is still held while these are filled. */
    if(!ls_heap_fits((sizeof(uint64_t) + 2 * sizeof(uint32_t) + 1) * (size_t)p->count)) {
        FURI_LOG_E(TAG, "no room to index %lu entries", (unsigned long)n);
        goto fail;
    }
    p->ids = malloc(sizeof(uint64_t) * p->count);
    p->offsets = malloc(sizeof(uint32_t) * p->count);
    p->lengths = malloc(sizeof(uint32_t) * p->count);
    p->runs = malloc(p->count);
    if(!p->ids || !p->offsets || !p->lengths || !p->runs) {
        FURI_LOG_E(TAG, "out of memory for %lu entries", (unsigned long)n);
        goto fail;
    }

    uint64_t last = 0;
    for(uint32_t i = 0; i < p->count; i++) {
        last += rd_varint(dir, (size_t)root_len, &q);
        p->ids[i] = last;
    }
    for(uint32_t i = 0; i < p->count; i++) {
        uint64_t r = rd_varint(dir, (size_t)root_len, &q);
        p->runs[i] = (r > 255) ? 255 : (uint8_t)r;
    }
    for(uint32_t i = 0; i < p->count; i++) {
        p->lengths[i] = (uint32_t)rd_varint(dir, (size_t)root_len, &q);
        /* With leaves these are directory sizes, not tiles; max_len is
           recomputed from the leaves below. */
        if(!p->leaf_buf && p->lengths[i] > p->max_len) p->max_len = p->lengths[i];
    }
    for(uint32_t i = 0; i < p->count; i++) {
        uint64_t v = rd_varint(dir, (size_t)root_len, &q);

        p->offsets[i] = (v == 0) ? (i ? p->offsets[i - 1] + p->lengths[i - 1] : 0)
                                 : (uint32_t)(v - 1);
    }

    free(dir);
    dir = NULL;

    p->tile_total = p->count;
    if(p->leaf_buf && !scan_leaf_tile_sizes(p)) goto fail;

    FURI_LOG_I(
        TAG,
        "opened %s: %lu tiles z%u-%u, max tile %lu b",
        path,
        (unsigned long)p->tile_total,
        p->min_zoom,
        p->max_zoom,
        (unsigned long)p->max_len);
    return p;

fail:
    free(dir);
    pmtiles_close(p);
    return NULL;
}

void pmtiles_close(PmTiles* p) {
    if(!p) return;
    free(p->ids);
    free(p->offsets);
    free(p->lengths);
    free(p->runs);
    free(p->leaf_buf);
    free(p->len_scratch);
    free(p->checks);
    if(p->file) {
        storage_file_close(p->file);
        storage_file_free(p->file);
    }
    if(p->storage) furi_record_close(RECORD_STORAGE);
    free(p);
}

/* Directory lookup with no tile read, so the map can ask whether a
   neighbour exists without paying for its data. */
static bool pm_locate(PmTiles* p, uint8_t z, uint32_t x, uint32_t y, uint32_t* off, uint32_t* len) {
    if(z < p->min_zoom || z > p->max_zoom) return false;

    uint64_t want = zxy_to_tileid(z, x, y);
    if(p->stream) return stream_find(p, want, off, len);
    int64_t idx = dir_upper(p->ids, p->count, want);
    if(idx < 0) return false;

    if(p->leaf_buf && p->runs[idx] == 0) {
        if(!leaf_load(p, p->offsets[idx], p->lengths[idx])) return false;
        return leaf_find(p, want, off, len);
    }

    uint32_t run = p->runs[idx] ? p->runs[idx] : 1;
    if(want >= p->ids[idx] + run) return false;
    *off = p->offsets[idx];
    *len = p->lengths[idx];
    return true;
}

bool pmtiles_has_tile(PmTiles* p, uint8_t z, uint32_t x, uint32_t y) {
    uint32_t off, len;
    return p && pm_locate(p, z, x, y, &off, &len);
}

bool pmtiles_get_tile(
    PmTiles* p,
    uint8_t z,
    uint32_t x,
    uint32_t y,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
    if(!p || !out || !out_len) return false;
    *out_len = 0;

    uint32_t off, len;
    if(!pm_locate(p, z, x, y, &off, &len)) return false;

    if(len > out_cap) {
        FURI_LOG_W(TAG, "tile %lu b exceeds buffer %u", (unsigned long)len, (unsigned)out_cap);
        return false;
    }
    if(!read_at(p->file, p->data_offset + off, out, len)) return false;
    *out_len = len;
    return true;
}

uint8_t pmtiles_min_zoom(const PmTiles* p) {
    return p ? p->min_zoom : 0;
}
uint8_t pmtiles_max_zoom(const PmTiles* p) {
    return p ? p->max_zoom : 0;
}
uint32_t pmtiles_tile_count(const PmTiles* p) {
    return p ? p->tile_total : 0;
}
uint32_t pmtiles_max_tile_len(const PmTiles* p) {
    return p ? p->max_len : 0;
}

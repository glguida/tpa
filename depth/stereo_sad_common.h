#ifndef DEPTH_STEREO_SAD_COMMON_H
#define DEPTH_STEREO_SAD_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "tpa/tpa.h"

#define DEPTH_STEREO_SAD_WIDTH 96u
#define DEPTH_STEREO_SAD_HEIGHT 64u
#define DEPTH_STEREO_SAD_DMAX 32u
#define DEPTH_STEREO_SAD_RADIUS 2u
#define DEPTH_STEREO_SAD_STRIPES 4u
#define DEPTH_STEREO_SAD_STRIPE_H 16u
#define DEPTH_STEREO_SAD_MAX_HALO_ROWS \
    (DEPTH_STEREO_SAD_STRIPE_H + 2u * DEPTH_STEREO_SAD_RADIUS)
#define DEPTH_STEREO_SAD_INVALID 255u
#define DEPTH_STEREO_SAD_INVALID_COST 0xffffu
#define DEPTH_STEREO_SAD_SEED 0x5ad96320u
#define DEPTH_STEREO_SAD_VALID_PIXELS 3050u

#define DEPTH_STEREO_SAD_PACKET_HEADER_BYTES 64u
#define DEPTH_STEREO_SAD_INPUT_HEADER_WORDS 10u
#define DEPTH_STEREO_SAD_OUTPUT_HEADER_WORDS 8u
#define DEPTH_STEREO_SAD_INPUT_HEADER_PAD_BYTES \
    (DEPTH_STEREO_SAD_PACKET_HEADER_BYTES - \
     DEPTH_STEREO_SAD_INPUT_HEADER_WORDS * sizeof(uint32_t))
#define DEPTH_STEREO_SAD_OUTPUT_HEADER_PAD_BYTES \
    (DEPTH_STEREO_SAD_PACKET_HEADER_BYTES - \
     DEPTH_STEREO_SAD_OUTPUT_HEADER_WORDS * sizeof(uint32_t))

#define DEPTH_STEREO_SAD_INPUT_PIXELS \
    (DEPTH_STEREO_SAD_WIDTH * DEPTH_STEREO_SAD_MAX_HALO_ROWS)
#define DEPTH_STEREO_SAD_OUTPUT_PIXELS \
    (DEPTH_STEREO_SAD_WIDTH * DEPTH_STEREO_SAD_STRIPE_H)
#define DEPTH_STEREO_SAD_INPUT_PACKET_BYTES 3904u
#define DEPTH_STEREO_SAD_OUTPUT_PACKET_BYTES 1600u

#define DEPTH_STEREO_SAD_SOURCE_WS_BYTES 16384u
#define DEPTH_STEREO_SAD_WORKER_WS_BYTES 2048u
#define DEPTH_STEREO_SAD_CHECKER_WS_BYTES 128u

struct depth_stereo_sad_input_packet {
    uint32_t stripe;
    uint32_t y0;
    uint32_t stripe_h;
    uint32_t halo_y0;
    uint32_t halo_rows;
    uint32_t width;
    uint32_t height;
    uint32_t dmax;
    uint32_t radius;
    uint32_t seed;
    uint8_t header_pad[DEPTH_STEREO_SAD_INPUT_HEADER_PAD_BYTES];
    uint8_t left[DEPTH_STEREO_SAD_INPUT_PIXELS];
    uint8_t right[DEPTH_STEREO_SAD_INPUT_PIXELS];
} __attribute__((aligned(64)));

struct depth_stereo_sad_output_packet {
    uint32_t stripe;
    uint32_t y0;
    uint32_t stripe_h;
    uint32_t width;
    uint32_t height;
    uint32_t dmax;
    uint32_t radius;
    uint32_t valid_pixels;
    uint8_t header_pad[DEPTH_STEREO_SAD_OUTPUT_HEADER_PAD_BYTES];
    uint8_t disparity[DEPTH_STEREO_SAD_OUTPUT_PIXELS];
} __attribute__((aligned(64)));

TPA_STATIC_ASSERT(DEPTH_STEREO_SAD_WIDTH == 96u,
                  "stereo SAD demo width drifted");
TPA_STATIC_ASSERT(DEPTH_STEREO_SAD_HEIGHT == 64u,
                  "stereo SAD demo height drifted");
TPA_STATIC_ASSERT(DEPTH_STEREO_SAD_DMAX == 32u,
                  "stereo SAD demo dmax drifted");
TPA_STATIC_ASSERT(DEPTH_STEREO_SAD_RADIUS == 2u,
                  "stereo SAD demo radius drifted");
TPA_STATIC_ASSERT(DEPTH_STEREO_SAD_STRIPES * DEPTH_STEREO_SAD_STRIPE_H ==
                      DEPTH_STEREO_SAD_HEIGHT,
                  "stereo SAD stripes must cover the image");
TPA_STATIC_ASSERT(offsetof(struct depth_stereo_sad_input_packet, left) ==
                      DEPTH_STEREO_SAD_PACKET_HEADER_BYTES,
                  "stereo SAD input left payload must start after header");
TPA_STATIC_ASSERT(offsetof(struct depth_stereo_sad_input_packet, right) ==
                      DEPTH_STEREO_SAD_PACKET_HEADER_BYTES +
                          DEPTH_STEREO_SAD_INPUT_PIXELS,
                  "stereo SAD input right payload offset drifted");
TPA_STATIC_ASSERT(sizeof(struct depth_stereo_sad_input_packet) ==
                      DEPTH_STEREO_SAD_INPUT_PACKET_BYTES,
                  "stereo SAD input packet size drifted");
TPA_STATIC_ASSERT(offsetof(struct depth_stereo_sad_output_packet, disparity) ==
                      DEPTH_STEREO_SAD_PACKET_HEADER_BYTES,
                  "stereo SAD output payload must start after header");
TPA_STATIC_ASSERT(sizeof(struct depth_stereo_sad_output_packet) ==
                      DEPTH_STEREO_SAD_OUTPUT_PACKET_BYTES,
                  "stereo SAD output packet size drifted");

static inline uint32_t depth_stereo_sad_mix32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static inline uint8_t depth_stereo_sad_texture(uint32_t x, uint32_t y,
                                                uint32_t seed)
{
    uint32_t v = seed ^ ((x + 0x9e3779b9u) * 0x85ebca6bu) ^
                 ((y + 0xc2b2ae35u) * 0x27d4eb2fu);

    return (uint8_t)(depth_stereo_sad_mix32(v) & 0xffu);
}

static inline uint32_t depth_stereo_sad_disparity_y(uint32_t y)
{
    uint32_t band = (y * 3u) / DEPTH_STEREO_SAD_HEIGHT;

    if (band == 0u)
        return 6u;
    if (band == 1u)
        return 12u;
    return 18u;
}

static inline uint32_t depth_stereo_sad_abs_diff_u32(uint32_t a, uint32_t b)
{
    return a > b ? a - b : b - a;
}

static inline int depth_stereo_sad_in_band_guard(uint32_t y)
{
    return depth_stereo_sad_abs_diff_u32(y, 21u) <= DEPTH_STEREO_SAD_RADIUS ||
           depth_stereo_sad_abs_diff_u32(y, 42u) <= DEPTH_STEREO_SAD_RADIUS;
}

static inline int depth_stereo_sad_pixel_valid(uint32_t x, uint32_t y)
{
    if (x < (DEPTH_STEREO_SAD_DMAX - 1u) + DEPTH_STEREO_SAD_RADIUS)
        return 0;
    if (x >= DEPTH_STEREO_SAD_WIDTH - DEPTH_STEREO_SAD_RADIUS)
        return 0;
    if (y < DEPTH_STEREO_SAD_RADIUS)
        return 0;
    if (y >= DEPTH_STEREO_SAD_HEIGHT - DEPTH_STEREO_SAD_RADIUS)
        return 0;
    if (depth_stereo_sad_in_band_guard(y))
        return 0;
    return 1;
}

static inline uint8_t depth_stereo_sad_left_value(uint32_t x, uint32_t y)
{
    return depth_stereo_sad_texture(x, y, DEPTH_STEREO_SAD_SEED);
}

static inline uint8_t depth_stereo_sad_right_value(uint32_t x, uint32_t y)
{
    uint32_t d = depth_stereo_sad_disparity_y(y);
    uint32_t src_x = x + d;

    if (src_x < DEPTH_STEREO_SAD_WIDTH)
        return depth_stereo_sad_texture(src_x, y, DEPTH_STEREO_SAD_SEED);

    return depth_stereo_sad_texture(x + 7919u, y + 1543u,
                                    DEPTH_STEREO_SAD_SEED ^ 0x0bad5eedu);
}

static inline uint32_t depth_stereo_sad_stripe_y0(uint32_t stripe)
{
    return stripe * DEPTH_STEREO_SAD_STRIPE_H;
}

static inline uint32_t depth_stereo_sad_halo_y0(uint32_t stripe)
{
    uint32_t y0 = depth_stereo_sad_stripe_y0(stripe);

    return y0 > DEPTH_STEREO_SAD_RADIUS ? y0 - DEPTH_STEREO_SAD_RADIUS : 0u;
}

static inline uint32_t depth_stereo_sad_halo_rows(uint32_t stripe)
{
    uint32_t y0 = depth_stereo_sad_stripe_y0(stripe);
    uint32_t halo_y0 = depth_stereo_sad_halo_y0(stripe);
    uint32_t halo_y1 = y0 + DEPTH_STEREO_SAD_STRIPE_H +
                       DEPTH_STEREO_SAD_RADIUS;

    if (halo_y1 > DEPTH_STEREO_SAD_HEIGHT)
        halo_y1 = DEPTH_STEREO_SAD_HEIGHT;

    return halo_y1 - halo_y0;
}

static inline uint16_t depth_stereo_sad_cost_at(
    const uint8_t *left, const uint8_t *right, uint32_t x, uint32_t local_y,
    uint32_t d)
{
    uint32_t cost = 0u;

    for (uint32_t wy = local_y - DEPTH_STEREO_SAD_RADIUS;
         wy <= local_y + DEPTH_STEREO_SAD_RADIUS; wy++) {
        for (uint32_t wx = x - DEPTH_STEREO_SAD_RADIUS;
             wx <= x + DEPTH_STEREO_SAD_RADIUS; wx++) {
            uint8_t lv = left[(size_t)wy * DEPTH_STEREO_SAD_WIDTH + wx];
            uint8_t rv = right[(size_t)wy * DEPTH_STEREO_SAD_WIDTH +
                               (wx - d)];

            cost += lv > rv ? (uint32_t)(lv - rv) : (uint32_t)(rv - lv);
        }
    }

    return (uint16_t)cost;
}

static inline uint8_t depth_stereo_sad_best_disparity(
    const struct depth_stereo_sad_input_packet *pkt, uint32_t x,
    uint32_t local_y, uint16_t *best_cost)
{
    uint32_t best = UINT32_MAX;
    uint8_t best_d = 0u;

    for (uint32_t d = 0u; d < DEPTH_STEREO_SAD_DMAX; d++) {
        uint32_t cost = depth_stereo_sad_cost_at(pkt->left, pkt->right, x,
                                                 local_y, d);

        if (cost < best) {
            best = cost;
            best_d = (uint8_t)d;
        }
    }

    *best_cost = (uint16_t)best;
    return best_d;
}

static inline uint8_t depth_stereo_sad_expected_disparity(uint32_t x,
                                                           uint32_t y)
{
    if (!depth_stereo_sad_pixel_valid(x, y))
        return (uint8_t)DEPTH_STEREO_SAD_INVALID;

    return (uint8_t)depth_stereo_sad_disparity_y(y);
}

#endif /* DEPTH_STEREO_SAD_COMMON_H */

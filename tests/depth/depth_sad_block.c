#include <stdint.h>
#include <stddef.h>

#include "test.h"
#include "tpa/tpa.h"

#include "generated/sad_argmin_16x8.h"
#include "generated/sad_cost_5x5.h"
#include "generated/sad_demo_96x64_stripe0.h"

#define DEPTH_SAD_INVALID_COST 0xffffu

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

tpa_op_t depth_sad_cost_start(void);
tpa_op_t depth_sad_argmin_start(void);
tpa_op_t depth_sad_stripe_start(void);

static uint16_t sad_cost_at(const uint8_t *left, const uint8_t *right,
                            uint32_t width, uint32_t x, uint32_t y,
                            uint32_t d, uint32_t r)
{
    uint32_t cost = 0;

    for (uint32_t wy = y - r; wy <= y + r; ++wy) {
        for (uint32_t wx = x - r; wx <= x + r; ++wx) {
            uint8_t lv = left[(size_t)wy * width + wx];
            uint8_t rv = right[(size_t)wy * width + (wx - d)];

            cost += (lv > rv) ? (uint32_t)(lv - rv) : (uint32_t)(rv - lv);
        }
    }

    return (uint16_t)cost;
}

static int valid_pixel(uint32_t width, uint32_t height, uint32_t dmax,
                       uint32_t r, uint32_t x, uint32_t y)
{
    if (x < (dmax - 1u) + r)
        return 0;
    if (x >= width - r)
        return 0;
    if (y < r)
        return 0;
    if (y >= height - r)
        return 0;
    return 1;
}

static uint8_t best_disparity_at(const uint8_t *left, const uint8_t *right,
                                 uint32_t width, uint32_t x, uint32_t y,
                                 uint32_t dmax, uint32_t r,
                                 uint16_t *best_cost)
{
    uint32_t best = UINT32_MAX;
    uint8_t best_d = 0;

    for (uint32_t d = 0; d < dmax; ++d) {
        uint32_t cost = sad_cost_at(left, right, width, x, y, d, r);

        if (cost < best) {
            best = cost;
            best_d = (uint8_t)d;
        }
    }

    *best_cost = (uint16_t)best;
    return best_d;
}

static int check_shape(uint32_t width, uint32_t height, uint32_t dmax,
                       uint32_t radius, uint32_t invalid)
{
    return width > 0u && height > 0u && dmax > 0u && radius == 2u &&
           invalid == 255u;
}

static int check_expected_map(const uint8_t *left, const uint8_t *right,
                              const uint8_t *expected_disp,
                              const uint16_t *expected_cost,
                              uint32_t width, uint32_t image_h,
                              uint32_t out_y0, uint32_t out_h,
                              uint32_t halo_y0, uint32_t dmax,
                              uint32_t radius, uint32_t expected_valid)
{
    uint32_t valid_count = 0;

    for (uint32_t oy = 0; oy < out_h; ++oy) {
        uint32_t global_y = out_y0 + oy;
        uint32_t local_y = global_y - halo_y0;

        for (uint32_t x = 0; x < width; ++x) {
            size_t out_idx = (size_t)oy * width + x;
            uint8_t got_disp = 255u;
            uint16_t got_cost = DEPTH_SAD_INVALID_COST;

            if (valid_pixel(width, image_h, dmax, radius, x, global_y)) {
                got_disp = best_disparity_at(left, right, width, x, local_y,
                                             dmax, radius, &got_cost);
                ++valid_count;
            }

            if (got_disp != expected_disp[out_idx])
                return 0;
            if (got_cost != expected_cost[out_idx])
                return 0;
        }
    }

    return valid_count == expected_valid;
}

static int check_cost_case(void)
{
    uint16_t true_cost;
    uint16_t alt_cost;

    if (!check_shape(STEREO_SAD_COST_5X5_WIDTH,
                     STEREO_SAD_COST_5X5_HEIGHT,
                     STEREO_SAD_COST_5X5_DMAX,
                     STEREO_SAD_COST_5X5_RADIUS,
                     STEREO_SAD_COST_5X5_INVALID))
        return 0;

    if (ARRAY_LEN(stereo_sad_cost_5x5_left) !=
        STEREO_SAD_COST_5X5_LEFT_PAYLOAD_LEN)
        return 0;
    if (ARRAY_LEN(stereo_sad_cost_5x5_right) !=
        STEREO_SAD_COST_5X5_RIGHT_PAYLOAD_LEN)
        return 0;
    if (ARRAY_LEN(stereo_sad_cost_5x5_expected_disparity) !=
        STEREO_SAD_COST_5X5_EXPECTED_LEN)
        return 0;

    true_cost = sad_cost_at(stereo_sad_cost_5x5_left,
                            stereo_sad_cost_5x5_right,
                            STEREO_SAD_COST_5X5_WIDTH,
                            STEREO_SAD_COST_5X5_PROBE_X,
                            STEREO_SAD_COST_5X5_PROBE_Y,
                            STEREO_SAD_COST_5X5_PROBE_DISPARITY,
                            STEREO_SAD_COST_5X5_RADIUS);
    alt_cost = sad_cost_at(stereo_sad_cost_5x5_left,
                           stereo_sad_cost_5x5_right,
                           STEREO_SAD_COST_5X5_WIDTH,
                           STEREO_SAD_COST_5X5_PROBE_X,
                           STEREO_SAD_COST_5X5_PROBE_Y,
                           STEREO_SAD_COST_5X5_PROBE_ALT_DISPARITY,
                           STEREO_SAD_COST_5X5_RADIUS);

    if (true_cost != STEREO_SAD_COST_5X5_PROBE_TRUE_COST)
        return 0;
    if (alt_cost != STEREO_SAD_COST_5X5_PROBE_ALT_COST)
        return 0;
    if (alt_cost <= true_cost)
        return 0;

    return check_expected_map(stereo_sad_cost_5x5_left,
                              stereo_sad_cost_5x5_right,
                              stereo_sad_cost_5x5_expected_disparity,
                              stereo_sad_cost_5x5_expected_best_cost,
                              STEREO_SAD_COST_5X5_WIDTH,
                              STEREO_SAD_COST_5X5_HEIGHT,
                              STEREO_SAD_COST_5X5_STRIPE_Y0,
                              STEREO_SAD_COST_5X5_STRIPE_H,
                              STEREO_SAD_COST_5X5_HALO_Y0,
                              STEREO_SAD_COST_5X5_DMAX,
                              STEREO_SAD_COST_5X5_RADIUS,
                              STEREO_SAD_COST_5X5_VALID_OUTPUT_PIXELS);
}

static int check_argmin_case(void)
{
    if (!check_shape(STEREO_SAD_ARGMIN_16X8_WIDTH,
                     STEREO_SAD_ARGMIN_16X8_HEIGHT,
                     STEREO_SAD_ARGMIN_16X8_DMAX,
                     STEREO_SAD_ARGMIN_16X8_RADIUS,
                     STEREO_SAD_ARGMIN_16X8_INVALID))
        return 0;
    if (STEREO_SAD_ARGMIN_16X8_PROBE_DISPARITY != 3u)
        return 0;
    if (ARRAY_LEN(stereo_sad_argmin_16x8_left) !=
        STEREO_SAD_ARGMIN_16X8_LEFT_PAYLOAD_LEN)
        return 0;
    if (ARRAY_LEN(stereo_sad_argmin_16x8_expected_best_cost) !=
        STEREO_SAD_ARGMIN_16X8_EXPECTED_LEN)
        return 0;

    return check_expected_map(stereo_sad_argmin_16x8_left,
                              stereo_sad_argmin_16x8_right,
                              stereo_sad_argmin_16x8_expected_disparity,
                              stereo_sad_argmin_16x8_expected_best_cost,
                              STEREO_SAD_ARGMIN_16X8_WIDTH,
                              STEREO_SAD_ARGMIN_16X8_HEIGHT,
                              STEREO_SAD_ARGMIN_16X8_STRIPE_Y0,
                              STEREO_SAD_ARGMIN_16X8_STRIPE_H,
                              STEREO_SAD_ARGMIN_16X8_HALO_Y0,
                              STEREO_SAD_ARGMIN_16X8_DMAX,
                              STEREO_SAD_ARGMIN_16X8_RADIUS,
                              STEREO_SAD_ARGMIN_16X8_VALID_OUTPUT_PIXELS);
}

static int check_stripe_case(void)
{
    if (!check_shape(STEREO_SAD_DEMO_96X64_STRIPE0_WIDTH,
                     STEREO_SAD_DEMO_96X64_STRIPE0_HEIGHT,
                     STEREO_SAD_DEMO_96X64_STRIPE0_DMAX,
                     STEREO_SAD_DEMO_96X64_STRIPE0_RADIUS,
                     STEREO_SAD_DEMO_96X64_STRIPE0_INVALID))
        return 0;
    if (STEREO_SAD_DEMO_96X64_STRIPE0_STRIPE_Y0 != 0u ||
        STEREO_SAD_DEMO_96X64_STRIPE0_STRIPE_H != 16u ||
        STEREO_SAD_DEMO_96X64_STRIPE0_DMAX != 32u)
        return 0;
    if (ARRAY_LEN(stereo_sad_demo_96x64_stripe0_left) !=
        STEREO_SAD_DEMO_96X64_STRIPE0_LEFT_PAYLOAD_LEN)
        return 0;
    if (ARRAY_LEN(stereo_sad_demo_96x64_stripe0_right) !=
        STEREO_SAD_DEMO_96X64_STRIPE0_RIGHT_PAYLOAD_LEN)
        return 0;
    if (ARRAY_LEN(stereo_sad_demo_96x64_stripe0_expected_disparity) !=
        STEREO_SAD_DEMO_96X64_STRIPE0_EXPECTED_LEN)
        return 0;

    return check_expected_map(stereo_sad_demo_96x64_stripe0_left,
                              stereo_sad_demo_96x64_stripe0_right,
                              stereo_sad_demo_96x64_stripe0_expected_disparity,
                              stereo_sad_demo_96x64_stripe0_expected_best_cost,
                              STEREO_SAD_DEMO_96X64_STRIPE0_WIDTH,
                              STEREO_SAD_DEMO_96X64_STRIPE0_HEIGHT,
                              STEREO_SAD_DEMO_96X64_STRIPE0_STRIPE_Y0,
                              STEREO_SAD_DEMO_96X64_STRIPE0_STRIPE_H,
                              STEREO_SAD_DEMO_96X64_STRIPE0_HALO_Y0,
                              STEREO_SAD_DEMO_96X64_STRIPE0_DMAX,
                              STEREO_SAD_DEMO_96X64_STRIPE0_RADIUS,
                              STEREO_SAD_DEMO_96X64_STRIPE0_VALID_OUTPUT_PIXELS);
}

tpa_op_t depth_sad_cost_start(void)
{
    if (check_cost_case()) {
        TEST_PASS;
        return tpa_stop();
    }

    TEST_FAIL;
    return tpa_stop();
}

tpa_op_t depth_sad_argmin_start(void)
{
    if (check_argmin_case()) {
        TEST_PASS;
        return tpa_stop();
    }

    TEST_FAIL;
    return tpa_stop();
}

tpa_op_t depth_sad_stripe_start(void)
{
    if (check_stripe_case()) {
        TEST_PASS;
        return tpa_stop();
    }

    TEST_FAIL;
    return tpa_stop();
}

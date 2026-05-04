#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#include "stereo_sad_common.h"

struct depth_stereo_sad_source_ws {
    struct depth_stereo_sad_input_packet packet[DEPTH_STEREO_SAD_STRIPES]
        __attribute__((aligned(64)));
    uint32_t next_stripe;
};

struct depth_stereo_sad_worker_ws {
    struct depth_stereo_sad_output_packet out __attribute__((aligned(64)));
    struct depth_stereo_sad_input_packet *in;
    uint32_t in_len;
};

struct depth_stereo_sad_checker_ws {
    struct depth_stereo_sad_output_packet *in;
    uint32_t in_len;
    uint32_t next_port;
    uint32_t received_mask;
    uint32_t valid_pixels;
    uint32_t mismatches;
};

TPA_STATIC_ASSERT(sizeof(struct depth_stereo_sad_source_ws) <=
                      DEPTH_STEREO_SAD_SOURCE_WS_BYTES,
                  "stereo SAD source manifest workspace too small");
TPA_STATIC_ASSERT(sizeof(struct depth_stereo_sad_worker_ws) <=
                      DEPTH_STEREO_SAD_WORKER_WS_BYTES,
                  "stereo SAD worker manifest workspace too small");
TPA_STATIC_ASSERT(sizeof(struct depth_stereo_sad_checker_ws) <=
                      DEPTH_STEREO_SAD_CHECKER_WS_BYTES,
                  "stereo SAD checker manifest workspace too small");

TPA_PROC_MEM_META(depth_stereo_sad_source_meta, 1401u, 0u);
TPA_PROC_MEM_META(depth_stereo_sad_worker_meta, 1402u, 0u);
TPA_PROC_MEM_META(depth_stereo_sad_checker_meta, 1403u, 0u);

static void depth_stereo_sad_fail(void)
{
    TEST_FAIL;
}

static void depth_stereo_sad_clear_input_packet(
    struct depth_stereo_sad_input_packet *pkt)
{
    for (uint32_t i = 0u; i < DEPTH_STEREO_SAD_INPUT_HEADER_PAD_BYTES; i++)
        pkt->header_pad[i] = 0u;
    for (uint32_t i = 0u; i < DEPTH_STEREO_SAD_INPUT_PIXELS; i++) {
        pkt->left[i] = 0u;
        pkt->right[i] = 0u;
    }
}

static void depth_stereo_sad_clear_output_packet(
    struct depth_stereo_sad_output_packet *pkt)
{
    for (uint32_t i = 0u; i < DEPTH_STEREO_SAD_OUTPUT_HEADER_PAD_BYTES; i++)
        pkt->header_pad[i] = 0u;
    for (uint32_t i = 0u; i < DEPTH_STEREO_SAD_OUTPUT_PIXELS; i++)
        pkt->disparity[i] = (uint8_t)DEPTH_STEREO_SAD_INVALID;
}

static void depth_stereo_sad_fill_input_packet(
    struct depth_stereo_sad_input_packet *pkt, uint32_t stripe)
{
    uint32_t y0 = depth_stereo_sad_stripe_y0(stripe);
    uint32_t halo_y0 = depth_stereo_sad_halo_y0(stripe);
    uint32_t halo_rows = depth_stereo_sad_halo_rows(stripe);

    depth_stereo_sad_clear_input_packet(pkt);
    pkt->stripe = stripe;
    pkt->y0 = y0;
    pkt->stripe_h = DEPTH_STEREO_SAD_STRIPE_H;
    pkt->halo_y0 = halo_y0;
    pkt->halo_rows = halo_rows;
    pkt->width = DEPTH_STEREO_SAD_WIDTH;
    pkt->height = DEPTH_STEREO_SAD_HEIGHT;
    pkt->dmax = DEPTH_STEREO_SAD_DMAX;
    pkt->radius = DEPTH_STEREO_SAD_RADIUS;
    pkt->seed = DEPTH_STEREO_SAD_SEED;

    for (uint32_t row = 0u; row < halo_rows; row++) {
        uint32_t y = halo_y0 + row;

        for (uint32_t x = 0u; x < DEPTH_STEREO_SAD_WIDTH; x++) {
            size_t idx = (size_t)row * DEPTH_STEREO_SAD_WIDTH + x;

            pkt->left[idx] = depth_stereo_sad_left_value(x, y);
            pkt->right[idx] = depth_stereo_sad_right_value(x, y);
        }
    }
}

static int depth_stereo_sad_input_valid(
    const struct depth_stereo_sad_input_packet *pkt)
{
    if (!pkt || pkt->stripe >= DEPTH_STEREO_SAD_STRIPES)
        return 0;
    if (pkt->y0 != depth_stereo_sad_stripe_y0(pkt->stripe))
        return 0;
    if (pkt->stripe_h != DEPTH_STEREO_SAD_STRIPE_H)
        return 0;
    if (pkt->halo_y0 != depth_stereo_sad_halo_y0(pkt->stripe))
        return 0;
    if (pkt->halo_rows != depth_stereo_sad_halo_rows(pkt->stripe))
        return 0;
    if (pkt->width != DEPTH_STEREO_SAD_WIDTH ||
        pkt->height != DEPTH_STEREO_SAD_HEIGHT ||
        pkt->dmax != DEPTH_STEREO_SAD_DMAX ||
        pkt->radius != DEPTH_STEREO_SAD_RADIUS ||
        pkt->seed != DEPTH_STEREO_SAD_SEED)
        return 0;
    return 1;
}

static void depth_stereo_sad_compute_stripe(
    const struct depth_stereo_sad_input_packet *in,
    struct depth_stereo_sad_output_packet *out)
{
    uint32_t valid_pixels = 0u;

    depth_stereo_sad_clear_output_packet(out);
    out->stripe = in->stripe;
    out->y0 = in->y0;
    out->stripe_h = in->stripe_h;
    out->width = DEPTH_STEREO_SAD_WIDTH;
    out->height = DEPTH_STEREO_SAD_HEIGHT;
    out->dmax = DEPTH_STEREO_SAD_DMAX;
    out->radius = DEPTH_STEREO_SAD_RADIUS;

    for (uint32_t oy = 0u; oy < DEPTH_STEREO_SAD_STRIPE_H; oy++) {
        uint32_t y = in->y0 + oy;
        uint32_t local_y = y - in->halo_y0;

        for (uint32_t x = 0u; x < DEPTH_STEREO_SAD_WIDTH; x++) {
            size_t out_idx = (size_t)oy * DEPTH_STEREO_SAD_WIDTH + x;
            uint8_t disp = (uint8_t)DEPTH_STEREO_SAD_INVALID;

            if (depth_stereo_sad_pixel_valid(x, y)) {
                uint16_t best_cost;

                disp = depth_stereo_sad_best_disparity(in, x, local_y,
                                                       &best_cost);
                (void)best_cost;
                valid_pixels++;
            }

            out->disparity[out_idx] = disp;
        }
    }

    out->valid_pixels = valid_pixels;
}

static int depth_stereo_sad_output_valid(
    const struct depth_stereo_sad_output_packet *pkt, uint32_t port)
{
    if (!pkt || pkt->stripe != port || pkt->stripe >= DEPTH_STEREO_SAD_STRIPES)
        return 0;
    if (pkt->y0 != depth_stereo_sad_stripe_y0(pkt->stripe))
        return 0;
    if (pkt->stripe_h != DEPTH_STEREO_SAD_STRIPE_H)
        return 0;
    if (pkt->width != DEPTH_STEREO_SAD_WIDTH ||
        pkt->height != DEPTH_STEREO_SAD_HEIGHT ||
        pkt->dmax != DEPTH_STEREO_SAD_DMAX ||
        pkt->radius != DEPTH_STEREO_SAD_RADIUS)
        return 0;
    return 1;
}

static int depth_stereo_sad_check_output(
    const struct depth_stereo_sad_output_packet *pkt,
    uint32_t *valid_pixels)
{
    uint32_t valid = 0u;

    for (uint32_t oy = 0u; oy < DEPTH_STEREO_SAD_STRIPE_H; oy++) {
        uint32_t y = pkt->y0 + oy;

        for (uint32_t x = 0u; x < DEPTH_STEREO_SAD_WIDTH; x++) {
            size_t idx = (size_t)oy * DEPTH_STEREO_SAD_WIDTH + x;
            uint8_t expect = depth_stereo_sad_expected_disparity(x, y);

            if (expect != DEPTH_STEREO_SAD_INVALID)
                valid++;
            if (pkt->disparity[idx] != expect)
                return 0;
        }
    }

    if (pkt->valid_pixels != valid)
        return 0;

    *valid_pixels = valid;
    return 1;
}

tpa_op_t depth_stereo_sad_source_start(void);
tpa_op_t depth_stereo_sad_source_send(void);
tpa_op_t depth_stereo_sad_source_after_send(void);
tpa_op_t depth_stereo_sad_worker_recv(void);
tpa_op_t depth_stereo_sad_worker_send(void);
tpa_op_t depth_stereo_sad_worker_done(void);
tpa_op_t depth_stereo_sad_checker_start(void);
tpa_op_t depth_stereo_sad_checker_recv_next(void);
tpa_op_t depth_stereo_sad_checker_after_recv(void);

tpa_op_t depth_stereo_sad_source_start(void)
{
    struct depth_stereo_sad_source_ws *w = tpa_ws();

    if (!w) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    for (uint32_t stripe = 0u; stripe < DEPTH_STEREO_SAD_STRIPES; stripe++)
        depth_stereo_sad_fill_input_packet(&w->packet[stripe], stripe);

    w->next_stripe = 0u;
    return depth_stereo_sad_source_send();
}

tpa_op_t depth_stereo_sad_source_send(void)
{
    struct depth_stereo_sad_source_ws *w = tpa_ws();
    struct tpa_channel *ch;

    if (!w || w->next_stripe >= DEPTH_STEREO_SAD_STRIPES) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    ch = tpa_chan((uint16_t)w->next_stripe);
    if (!ch) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    return tpa_send(ch, &w->packet[w->next_stripe],
                    sizeof(w->packet[w->next_stripe]),
                    depth_stereo_sad_source_after_send);
}

tpa_op_t depth_stereo_sad_source_after_send(void)
{
    struct depth_stereo_sad_source_ws *w = tpa_ws();

    if (!w) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    w->next_stripe++;
    if (w->next_stripe < DEPTH_STEREO_SAD_STRIPES)
        return depth_stereo_sad_source_send();

    return tpa_stop();
}

tpa_op_t depth_stereo_sad_worker_recv(void)
{
    struct depth_stereo_sad_worker_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(0);

    if (!w || !ch) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->in, &w->in_len,
                    depth_stereo_sad_worker_send);
}

tpa_op_t depth_stereo_sad_worker_send(void)
{
    struct depth_stereo_sad_worker_ws *w = tpa_ws();
    struct tpa_channel *ch = tpa_chan(1);

    if (!w || !ch || w->in_len != sizeof(*w->in) ||
        !depth_stereo_sad_input_valid(w->in)) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    depth_stereo_sad_compute_stripe(w->in, &w->out);
    return tpa_send(ch, &w->out, sizeof(w->out),
                    depth_stereo_sad_worker_done);
}

tpa_op_t depth_stereo_sad_worker_done(void)
{
    return tpa_stop();
}

tpa_op_t depth_stereo_sad_checker_start(void)
{
    struct depth_stereo_sad_checker_ws *w = tpa_ws();

    if (!w) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    w->next_port = 0u;
    w->received_mask = 0u;
    w->valid_pixels = 0u;
    w->mismatches = 0u;
    return depth_stereo_sad_checker_recv_next();
}

tpa_op_t depth_stereo_sad_checker_recv_next(void)
{
    struct depth_stereo_sad_checker_ws *w = tpa_ws();
    struct tpa_channel *ch;

    if (!w || w->next_port >= DEPTH_STEREO_SAD_STRIPES) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    ch = tpa_chan((uint16_t)w->next_port);
    if (!ch) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    return tpa_recv(ch, (void **)&w->in, &w->in_len,
                    depth_stereo_sad_checker_after_recv);
}

tpa_op_t depth_stereo_sad_checker_after_recv(void)
{
    struct depth_stereo_sad_checker_ws *w = tpa_ws();
    uint32_t valid = 0u;
    uint32_t port;
    uint32_t all_stripes = (1u << DEPTH_STEREO_SAD_STRIPES) - 1u;

    if (!w || w->next_port >= DEPTH_STEREO_SAD_STRIPES) {
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    port = w->next_port;
    if (w->in_len != sizeof(*w->in) ||
        !depth_stereo_sad_output_valid(w->in, port) ||
        !depth_stereo_sad_check_output(w->in, &valid)) {
        w->mismatches++;
        depth_stereo_sad_fail();
        return tpa_stop();
    }

    w->valid_pixels += valid;
    w->received_mask |= 1u << port;
    w->next_port++;
    if (w->next_port < DEPTH_STEREO_SAD_STRIPES)
        return depth_stereo_sad_checker_recv_next();

    if (w->received_mask == all_stripes &&
        w->valid_pixels == DEPTH_STEREO_SAD_VALID_PIXELS &&
        w->mismatches == 0u) {
        TEST_PASS;
        return tpa_stop();
    }

    depth_stereo_sad_fail();
    return tpa_stop();
}

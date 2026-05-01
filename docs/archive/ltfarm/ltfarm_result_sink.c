#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test.h"
#include "tpa/tpa.h"

#include "ltfarm_worker_core.h"

typedef struct {
    const uint8_t *batch;
    uint32_t batch_len;
} ltfarm_result_sink_ws_t;

volatile ltfarm_hash_result_t ltfarm_hash_result __attribute__((aligned(64)));

tpa_op_t ltfarm_result_sink_start(void);
static tpa_op_t ltfarm_result_sink_done(void);

TPA_PROC_MEM_META(ltfarm_result_sink_meta, 602u, 0u);

static __attribute__((noreturn)) void ltfarm_sink_fail(const char *msg)
{
    ltfarm_diag_puts(msg);
    TEST_FAIL;
    __builtin_unreachable();
}

static void flush_region(const volatile void *ptr, size_t len)
{
    arch_flush(ptr, len);
}

tpa_op_t ltfarm_result_sink_start(void)
{
    ltfarm_result_sink_ws_t *w = tpa_ws();

    memset((void *)&ltfarm_hash_result, 0, sizeof(ltfarm_hash_result));
    if (!w)
        ltfarm_sink_fail("LTSNKWS\n");

    return tpa_recv(tpa_chan(0), (void **)&w->batch, &w->batch_len,
                    ltfarm_result_sink_done);
}

static tpa_op_t ltfarm_result_sink_done(void)
{
    ltfarm_result_sink_ws_t *w = tpa_ws();

    if (!w)
        ltfarm_sink_fail("LTSNKW\n");
    if (!w->batch)
        ltfarm_sink_fail("LTSNKH\n");
    if (w->batch_len != sizeof(ltfarm_hash_batch_t))
        ltfarm_sink_fail("LTSNKL\n");

    {
        const ltfarm_hash_batch_t *batch =
            (const ltfarm_hash_batch_t *)(const void *)w->batch;

        if (batch->count > LTFARM_SCRYPT_LANES)
            ltfarm_sink_fail("LTSNKC\n");
        memcpy((void *)ltfarm_hash_result.hash, batch->hash,
               sizeof(ltfarm_hash_result.hash));
        ltfarm_hash_result.count = batch->count;
    }
    ltfarm_hash_result.magic = LTFARM_SCRYPT_RESULT_MAGIC;
    ltfarm_hash_result.status = LTFARM_WORKER_STATUS_DONE;
    ltfarm_hash_result.ready = 1u;
    flush_region(&ltfarm_hash_result, sizeof(ltfarm_hash_result));
    TEST_PASS;
    return tpa_stop();
}

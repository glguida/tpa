#include <stddef.h>
#include <stdint.h>

#include "test.h"
#include "tpa/tpa.h"

#include "ltfarm_worker_core.h"

volatile ltfarm_job_input_t ltfarm_job_input __attribute__((aligned(64)));

tpa_op_t ltfarm_job_source_start(void);
static tpa_op_t ltfarm_job_source_done(void);

TPA_PROC_MEM_META(ltfarm_job_source_meta, 600u, 0u);

static void evict_region(const volatile void *ptr, size_t len)
{
    arch_evict(ptr, len);
}

tpa_op_t ltfarm_job_source_start(void)
{
    struct tpa_chan *ch = tpa_chan(0);
    const uint8_t *job = (const uint8_t *)(const void *)&ltfarm_job_input;

    if (!ch || !job) {
        ltfarm_diag_puts("LTSRC\n");
        TEST_FAIL;
        __builtin_unreachable();
    }

    evict_region(&ltfarm_job_input, sizeof(ltfarm_job_input));
    return tpa_send_const(ch, job, sizeof(ltfarm_job_input),
                          ltfarm_job_source_done);
}

static tpa_op_t ltfarm_job_source_done(void)
{
    return tpa_stop();
}

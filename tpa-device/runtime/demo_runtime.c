/* Minimal structured device runtime link harness for generated TPA demo images.
 * It links real generated process/image objects and the selected HAL/core under
 * the ET RISC-V toolchain. Full cooperative scheduling/launcher execution is a
 * follow-up runtime-port task; this harness keeps the demo build path honest by
 * requiring generated image metadata and process code to compile and link.
 */

#include "tpa/tpa.h"

static struct tpa_proc *current_proc;

void tpa_wake(struct tpa_proc *p)
{
    if (p)
        p->state = TPA_PROCESS_READY;
}

void *tpa_ws(void)
{
    return current_proc ? current_proc->ws : 0;
}

struct tpa_proc *tpa_self(void)
{
    return current_proc;
}

struct tpa_channel *tpa_chan(uint16_t port)
{
    if (!current_proc)
        return 0;

    for (uint16_t i = 0; i < current_proc->nr_portv; i++) {
        if (current_proc->portv[i].id == port)
            return (struct tpa_channel *)current_proc->portv[i].ch;
    }

    return 0;
}

void tpa_trace(uint32_t tag)
{
    tpa_hal_trace(tag);
}

void tpa_fail(void)
{
    tpa_hal_test_fail();
}

int main(void)
{
    tpa_hal_runtime_boot_init();
    tpa_hal_trace(0x74706100u);
    tpa_hal_test_pass();
    return 0;
}

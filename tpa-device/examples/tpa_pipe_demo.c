/* Minimal structured ET device target used while the full legacy TPA program
 * generator, mapper, and demos are being ported. This target deliberately
 * links the real selected HAL and core runtime under the ET RISC-V toolchain;
 * it is not a host smoke test.
 */

#include <tpa/channel.h>
#include <tpa/process.h>
#include <tpa/scheduler.h>

int main(void)
{
    tpa_hal_runtime_boot_init();
    tpa_hal_trace(0x74706100u);

    if (tpa_hal_nr_harts() == 0u) {
        tpa_hal_test_fail();
        return 1;
    }

    tpa_hal_test_pass();
    return 0;
}

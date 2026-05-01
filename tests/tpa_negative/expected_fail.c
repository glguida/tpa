#include "test.h"
#include "tpa/tpa.h"

tpa_op_t negative_expected_fail_start(void);

tpa_op_t negative_expected_fail_start(void)
{
    TEST_FAIL;
    return tpa_stop();
}

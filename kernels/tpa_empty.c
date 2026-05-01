#include "tpa/tpa.h"

tpa_op_t tpa_empty_start(void);

tpa_op_t tpa_empty_start(void)
{
    return tpa_stop();
}

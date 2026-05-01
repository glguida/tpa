/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#ifndef ERBIUM_TEST_H
#define ERBIUM_TEST_H

#include <stdint.h>

#include "tpa/hal.h"

/* On Erbium these magic values are recognized by the emulator:
 *   0x1FEED000 - Signal test PASS, hart becomes unavailable
 *   0x50BAD000 - Signal test FAIL, emulator stops with failure
 */

#define TEST_PASS do { \
    tpa_hal_test_pass(); \
} while(0)

#define TEST_FAIL do { \
    tpa_hal_test_fail(); \
} while(0)

#endif /* ERBIUM_TEST_H */

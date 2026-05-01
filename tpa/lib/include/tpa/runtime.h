/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#ifndef TPA_RUNTIME_H
#define TPA_RUNTIME_H

#include <stdint.h>

#include "tpa/process.h"

#ifdef __cplusplus
extern "C" {
#endif

void tpa_init(void);
void tpa_boot_hart(uint32_t hartid);
struct tpa_proc *tpa_runq_pop(uint32_t hartid);
void tpa_reload(uint32_t hartid);
void tpa_step(struct tpa_proc *p);
uint32_t tpa_done(void);
uint32_t tpa_failed(void);
void tpa_scheduler_main(uint32_t hartid);
void tpa_hart_main(void);

#ifdef __cplusplus
}
#endif

#endif /* TPA_RUNTIME_H */

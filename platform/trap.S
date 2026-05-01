/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Default trap handler for Erbium tests
* Located at 0x02009000 (4K aligned, second page of bootrom)
* Uses mscratch CSR to hold expected exception cause
*/

  .section traprom, "ax", @progbits
  .global default_trap_handler

default_trap_handler:
  csrr t0, mcause
  csrr t1, mscratch
  beqz t1, trap_unexpected
  bne  t0, t1, trap_unexpected

trap_expected:
  li   a7, 0x1FEED000         /* PASS */
  j    trap_done

trap_unexpected:
  csrr a0, mcause
  csrr a1, mepc
  csrr a2, mtval
  csrw validation0, a0
  csrw validation0, a1
  csrw validation0, a2
  li   a7, 0x50BAD000         /* FAIL */

trap_done:
  csrw validation0, a7
  wfi
  j    trap_done

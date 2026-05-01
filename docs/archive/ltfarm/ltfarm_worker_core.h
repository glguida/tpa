#ifndef LTFARM_WORKER_CORE_H
#define LTFARM_WORKER_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LTFARM_SCRYPT_LANES = 8u,
    LTFARM_SCRYPT_INPUT_BYTES = 80u,
    LTFARM_SCRYPT_HASH_BYTES = 32u,
    LTFARM_SCRYPT_JOB_BATCH_BYTES = 8u + (LTFARM_SCRYPT_LANES * LTFARM_SCRYPT_INPUT_BYTES),
    LTFARM_SCRYPT_HASH_BATCH_BYTES = 8u + (LTFARM_SCRYPT_LANES * LTFARM_SCRYPT_HASH_BYTES),
    LTFARM_SCRYPT_ROM_ENTRY_WORDS = 32u,
    LTFARM_SCRYPT_ROM_ENTRIES = 1024u,
    LTFARM_SCRYPT_SCRATCHPAD_BYTES =
        (LTFARM_SCRYPT_ROM_ENTRIES *
         LTFARM_SCRYPT_ROM_ENTRY_WORDS *
         LTFARM_SCRYPT_LANES *
         sizeof(uint32_t)) + 63u,
    LTFARM_CACHE_LINE_BYTES = 64u,
};

#define LTFARM_SCRYPT_RESULT_MAGIC UINT64_C(0x4c54465343525950)

enum {
    LTFARM_WORKER_STATUS_IDLE = 0u,
    LTFARM_WORKER_STATUS_DONE = 1u,
};

typedef struct {
    uint32_t count;
    uint32_t reserved;
    uint8_t header[LTFARM_SCRYPT_LANES][LTFARM_SCRYPT_INPUT_BYTES];
} ltfarm_job_input_t;

typedef struct {
    uint32_t count;
    uint32_t reserved;
    uint8_t hash[LTFARM_SCRYPT_LANES][LTFARM_SCRYPT_HASH_BYTES];
} ltfarm_hash_batch_t;

typedef struct {
    uint64_t magic;
    uint32_t ready;
    uint32_t status;
    uint32_t count;
    uint32_t reserved;
    uint8_t hash[LTFARM_SCRYPT_LANES][LTFARM_SCRYPT_HASH_BYTES];
} ltfarm_hash_result_t;

extern volatile ltfarm_job_input_t ltfarm_job_input;
extern volatile ltfarm_hash_result_t ltfarm_hash_result;

static inline void ltfarm_diag_putc(char c)
{
    uint64_t v = (1ull << 56) | (uint8_t)c;

    asm volatile("csrw validation1, %0" :: "r"(v) : "memory");
}

static inline void ltfarm_diag_puts(const char *s)
{
    while (*s)
        ltfarm_diag_putc(*s++);
}

#ifdef __cplusplus
}
#endif

#endif

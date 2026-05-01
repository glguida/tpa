/*
 * Litecoin-style generic scrypt(1024,1,1,32) worker core for TPA.
 *
 * This file is the actual worker process implementation. It receives one
 * 80-byte header on its input port, computes the 32-byte scrypt hash, and
 * sends that hash on its output port.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test.h"
#include "tpa/tpa.h"

#include "ltfarm_worker_core.h"

typedef struct {
    uint8_t *in;
    uint32_t in_len;
    ltfarm_hash_batch_t out;
} ltfarm_scrypt_worker_ws_t;

tpa_op_t ltfarm_scrypt_worker_start(void);
static tpa_op_t ltfarm_scrypt_worker_run(void);
static tpa_op_t ltfarm_scrypt_worker_done(void);

TPA_PROC_MEM_META(ltfarm_scrypt_worker_meta, 601u, LTFARM_SCRYPT_SCRATCHPAD_BYTES);

static uint8_t worker_scratchpad[LTFARM_SCRYPT_SCRATCHPAD_BYTES]
    __attribute__((aligned(64)));

static const uint32_t kSha256Init[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
};

static const uint32_t kSha256KVec[64][LTFARM_SCRYPT_LANES] = {
    {0x428a2f98u, 0x428a2f98u, 0x428a2f98u, 0x428a2f98u, 0x428a2f98u, 0x428a2f98u, 0x428a2f98u, 0x428a2f98u},
    {0x71374491u, 0x71374491u, 0x71374491u, 0x71374491u, 0x71374491u, 0x71374491u, 0x71374491u, 0x71374491u},
    {0xb5c0fbcfu, 0xb5c0fbcfu, 0xb5c0fbcfu, 0xb5c0fbcfu, 0xb5c0fbcfu, 0xb5c0fbcfu, 0xb5c0fbcfu, 0xb5c0fbcfu},
    {0xe9b5dba5u, 0xe9b5dba5u, 0xe9b5dba5u, 0xe9b5dba5u, 0xe9b5dba5u, 0xe9b5dba5u, 0xe9b5dba5u, 0xe9b5dba5u},
    {0x3956c25bu, 0x3956c25bu, 0x3956c25bu, 0x3956c25bu, 0x3956c25bu, 0x3956c25bu, 0x3956c25bu, 0x3956c25bu},
    {0x59f111f1u, 0x59f111f1u, 0x59f111f1u, 0x59f111f1u, 0x59f111f1u, 0x59f111f1u, 0x59f111f1u, 0x59f111f1u},
    {0x923f82a4u, 0x923f82a4u, 0x923f82a4u, 0x923f82a4u, 0x923f82a4u, 0x923f82a4u, 0x923f82a4u, 0x923f82a4u},
    {0xab1c5ed5u, 0xab1c5ed5u, 0xab1c5ed5u, 0xab1c5ed5u, 0xab1c5ed5u, 0xab1c5ed5u, 0xab1c5ed5u, 0xab1c5ed5u},
    {0xd807aa98u, 0xd807aa98u, 0xd807aa98u, 0xd807aa98u, 0xd807aa98u, 0xd807aa98u, 0xd807aa98u, 0xd807aa98u},
    {0x12835b01u, 0x12835b01u, 0x12835b01u, 0x12835b01u, 0x12835b01u, 0x12835b01u, 0x12835b01u, 0x12835b01u},
    {0x243185beu, 0x243185beu, 0x243185beu, 0x243185beu, 0x243185beu, 0x243185beu, 0x243185beu, 0x243185beu},
    {0x550c7dc3u, 0x550c7dc3u, 0x550c7dc3u, 0x550c7dc3u, 0x550c7dc3u, 0x550c7dc3u, 0x550c7dc3u, 0x550c7dc3u},
    {0x72be5d74u, 0x72be5d74u, 0x72be5d74u, 0x72be5d74u, 0x72be5d74u, 0x72be5d74u, 0x72be5d74u, 0x72be5d74u},
    {0x80deb1feu, 0x80deb1feu, 0x80deb1feu, 0x80deb1feu, 0x80deb1feu, 0x80deb1feu, 0x80deb1feu, 0x80deb1feu},
    {0x9bdc06a7u, 0x9bdc06a7u, 0x9bdc06a7u, 0x9bdc06a7u, 0x9bdc06a7u, 0x9bdc06a7u, 0x9bdc06a7u, 0x9bdc06a7u},
    {0xc19bf174u, 0xc19bf174u, 0xc19bf174u, 0xc19bf174u, 0xc19bf174u, 0xc19bf174u, 0xc19bf174u, 0xc19bf174u},
    {0xe49b69c1u, 0xe49b69c1u, 0xe49b69c1u, 0xe49b69c1u, 0xe49b69c1u, 0xe49b69c1u, 0xe49b69c1u, 0xe49b69c1u},
    {0xefbe4786u, 0xefbe4786u, 0xefbe4786u, 0xefbe4786u, 0xefbe4786u, 0xefbe4786u, 0xefbe4786u, 0xefbe4786u},
    {0x0fc19dc6u, 0x0fc19dc6u, 0x0fc19dc6u, 0x0fc19dc6u, 0x0fc19dc6u, 0x0fc19dc6u, 0x0fc19dc6u, 0x0fc19dc6u},
    {0x240ca1ccu, 0x240ca1ccu, 0x240ca1ccu, 0x240ca1ccu, 0x240ca1ccu, 0x240ca1ccu, 0x240ca1ccu, 0x240ca1ccu},
    {0x2de92c6fu, 0x2de92c6fu, 0x2de92c6fu, 0x2de92c6fu, 0x2de92c6fu, 0x2de92c6fu, 0x2de92c6fu, 0x2de92c6fu},
    {0x4a7484aau, 0x4a7484aau, 0x4a7484aau, 0x4a7484aau, 0x4a7484aau, 0x4a7484aau, 0x4a7484aau, 0x4a7484aau},
    {0x5cb0a9dcu, 0x5cb0a9dcu, 0x5cb0a9dcu, 0x5cb0a9dcu, 0x5cb0a9dcu, 0x5cb0a9dcu, 0x5cb0a9dcu, 0x5cb0a9dcu},
    {0x76f988dau, 0x76f988dau, 0x76f988dau, 0x76f988dau, 0x76f988dau, 0x76f988dau, 0x76f988dau, 0x76f988dau},
    {0x983e5152u, 0x983e5152u, 0x983e5152u, 0x983e5152u, 0x983e5152u, 0x983e5152u, 0x983e5152u, 0x983e5152u},
    {0xa831c66du, 0xa831c66du, 0xa831c66du, 0xa831c66du, 0xa831c66du, 0xa831c66du, 0xa831c66du, 0xa831c66du},
    {0xb00327c8u, 0xb00327c8u, 0xb00327c8u, 0xb00327c8u, 0xb00327c8u, 0xb00327c8u, 0xb00327c8u, 0xb00327c8u},
    {0xbf597fc7u, 0xbf597fc7u, 0xbf597fc7u, 0xbf597fc7u, 0xbf597fc7u, 0xbf597fc7u, 0xbf597fc7u, 0xbf597fc7u},
    {0xc6e00bf3u, 0xc6e00bf3u, 0xc6e00bf3u, 0xc6e00bf3u, 0xc6e00bf3u, 0xc6e00bf3u, 0xc6e00bf3u, 0xc6e00bf3u},
    {0xd5a79147u, 0xd5a79147u, 0xd5a79147u, 0xd5a79147u, 0xd5a79147u, 0xd5a79147u, 0xd5a79147u, 0xd5a79147u},
    {0x06ca6351u, 0x06ca6351u, 0x06ca6351u, 0x06ca6351u, 0x06ca6351u, 0x06ca6351u, 0x06ca6351u, 0x06ca6351u},
    {0x14292967u, 0x14292967u, 0x14292967u, 0x14292967u, 0x14292967u, 0x14292967u, 0x14292967u, 0x14292967u},
    {0x27b70a85u, 0x27b70a85u, 0x27b70a85u, 0x27b70a85u, 0x27b70a85u, 0x27b70a85u, 0x27b70a85u, 0x27b70a85u},
    {0x2e1b2138u, 0x2e1b2138u, 0x2e1b2138u, 0x2e1b2138u, 0x2e1b2138u, 0x2e1b2138u, 0x2e1b2138u, 0x2e1b2138u},
    {0x4d2c6dfcu, 0x4d2c6dfcu, 0x4d2c6dfcu, 0x4d2c6dfcu, 0x4d2c6dfcu, 0x4d2c6dfcu, 0x4d2c6dfcu, 0x4d2c6dfcu},
    {0x53380d13u, 0x53380d13u, 0x53380d13u, 0x53380d13u, 0x53380d13u, 0x53380d13u, 0x53380d13u, 0x53380d13u},
    {0x650a7354u, 0x650a7354u, 0x650a7354u, 0x650a7354u, 0x650a7354u, 0x650a7354u, 0x650a7354u, 0x650a7354u},
    {0x766a0abbu, 0x766a0abbu, 0x766a0abbu, 0x766a0abbu, 0x766a0abbu, 0x766a0abbu, 0x766a0abbu, 0x766a0abbu},
    {0x81c2c92eu, 0x81c2c92eu, 0x81c2c92eu, 0x81c2c92eu, 0x81c2c92eu, 0x81c2c92eu, 0x81c2c92eu, 0x81c2c92eu},
    {0x92722c85u, 0x92722c85u, 0x92722c85u, 0x92722c85u, 0x92722c85u, 0x92722c85u, 0x92722c85u, 0x92722c85u},
    {0xa2bfe8a1u, 0xa2bfe8a1u, 0xa2bfe8a1u, 0xa2bfe8a1u, 0xa2bfe8a1u, 0xa2bfe8a1u, 0xa2bfe8a1u, 0xa2bfe8a1u},
    {0xa81a664bu, 0xa81a664bu, 0xa81a664bu, 0xa81a664bu, 0xa81a664bu, 0xa81a664bu, 0xa81a664bu, 0xa81a664bu},
    {0xc24b8b70u, 0xc24b8b70u, 0xc24b8b70u, 0xc24b8b70u, 0xc24b8b70u, 0xc24b8b70u, 0xc24b8b70u, 0xc24b8b70u},
    {0xc76c51a3u, 0xc76c51a3u, 0xc76c51a3u, 0xc76c51a3u, 0xc76c51a3u, 0xc76c51a3u, 0xc76c51a3u, 0xc76c51a3u},
    {0xd192e819u, 0xd192e819u, 0xd192e819u, 0xd192e819u, 0xd192e819u, 0xd192e819u, 0xd192e819u, 0xd192e819u},
    {0xd6990624u, 0xd6990624u, 0xd6990624u, 0xd6990624u, 0xd6990624u, 0xd6990624u, 0xd6990624u, 0xd6990624u},
    {0xf40e3585u, 0xf40e3585u, 0xf40e3585u, 0xf40e3585u, 0xf40e3585u, 0xf40e3585u, 0xf40e3585u, 0xf40e3585u},
    {0x106aa070u, 0x106aa070u, 0x106aa070u, 0x106aa070u, 0x106aa070u, 0x106aa070u, 0x106aa070u, 0x106aa070u},
    {0x19a4c116u, 0x19a4c116u, 0x19a4c116u, 0x19a4c116u, 0x19a4c116u, 0x19a4c116u, 0x19a4c116u, 0x19a4c116u},
    {0x1e376c08u, 0x1e376c08u, 0x1e376c08u, 0x1e376c08u, 0x1e376c08u, 0x1e376c08u, 0x1e376c08u, 0x1e376c08u},
    {0x2748774cu, 0x2748774cu, 0x2748774cu, 0x2748774cu, 0x2748774cu, 0x2748774cu, 0x2748774cu, 0x2748774cu},
    {0x34b0bcb5u, 0x34b0bcb5u, 0x34b0bcb5u, 0x34b0bcb5u, 0x34b0bcb5u, 0x34b0bcb5u, 0x34b0bcb5u, 0x34b0bcb5u},
    {0x391c0cb3u, 0x391c0cb3u, 0x391c0cb3u, 0x391c0cb3u, 0x391c0cb3u, 0x391c0cb3u, 0x391c0cb3u, 0x391c0cb3u},
    {0x4ed8aa4au, 0x4ed8aa4au, 0x4ed8aa4au, 0x4ed8aa4au, 0x4ed8aa4au, 0x4ed8aa4au, 0x4ed8aa4au, 0x4ed8aa4au},
    {0x5b9cca4fu, 0x5b9cca4fu, 0x5b9cca4fu, 0x5b9cca4fu, 0x5b9cca4fu, 0x5b9cca4fu, 0x5b9cca4fu, 0x5b9cca4fu},
    {0x682e6ff3u, 0x682e6ff3u, 0x682e6ff3u, 0x682e6ff3u, 0x682e6ff3u, 0x682e6ff3u, 0x682e6ff3u, 0x682e6ff3u},
    {0x748f82eeu, 0x748f82eeu, 0x748f82eeu, 0x748f82eeu, 0x748f82eeu, 0x748f82eeu, 0x748f82eeu, 0x748f82eeu},
    {0x78a5636fu, 0x78a5636fu, 0x78a5636fu, 0x78a5636fu, 0x78a5636fu, 0x78a5636fu, 0x78a5636fu, 0x78a5636fu},
    {0x84c87814u, 0x84c87814u, 0x84c87814u, 0x84c87814u, 0x84c87814u, 0x84c87814u, 0x84c87814u, 0x84c87814u},
    {0x8cc70208u, 0x8cc70208u, 0x8cc70208u, 0x8cc70208u, 0x8cc70208u, 0x8cc70208u, 0x8cc70208u, 0x8cc70208u},
    {0x90befffau, 0x90befffau, 0x90befffau, 0x90befffau, 0x90befffau, 0x90befffau, 0x90befffau, 0x90befffau},
    {0xa4506cebu, 0xa4506cebu, 0xa4506cebu, 0xa4506cebu, 0xa4506cebu, 0xa4506cebu, 0xa4506cebu, 0xa4506cebu},
    {0xbef9a3f7u, 0xbef9a3f7u, 0xbef9a3f7u, 0xbef9a3f7u, 0xbef9a3f7u, 0xbef9a3f7u, 0xbef9a3f7u, 0xbef9a3f7u},
    {0xc67178f2u, 0xc67178f2u, 0xc67178f2u, 0xc67178f2u, 0xc67178f2u, 0xc67178f2u, 0xc67178f2u, 0xc67178f2u},
};

static inline uint32_t be32dec(const void *pp)
{
    const uint8_t *p = (const uint8_t *)pp;
    return ((uint32_t)p[3]) |
           ((uint32_t)p[2] << 8) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[0] << 24);
}

static inline void be32enc(void *pp, uint32_t x)
{
    uint8_t *p = (uint8_t *)pp;
    p[0] = (uint8_t)(x >> 24);
    p[1] = (uint8_t)(x >> 16);
    p[2] = (uint8_t)(x >> 8);
    p[3] = (uint8_t)x;
}

static inline uint32_t le32dec(const void *pp)
{
    const uint8_t *p = (const uint8_t *)pp;
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline void le32enc(void *pp, uint32_t x)
{
    uint8_t *p = (uint8_t *)pp;
    p[0] = (uint8_t)x;
    p[1] = (uint8_t)(x >> 8);
    p[2] = (uint8_t)(x >> 16);
    p[3] = (uint8_t)(x >> 24);
}

static __attribute__((noreturn)) void ltfarm_fail(const char *msg)
{
    ltfarm_diag_puts(msg);
    TEST_FAIL;
    __builtin_unreachable();
}

static inline __attribute__((always_inline))
void vec_enable_all_lanes(void)
{
    __asm__ __volatile__(
        "mov.m.x m0, zero, 0xff\n"
        :
        :
        : "memory");
}

static inline __attribute__((always_inline))
void vec_copy_u32x8(uint32_t dst[8], const uint32_t src[8])
{
    __asm__ __volatile__(
        "flw.ps  f0, 0(%[src])\n"
        "fsw.ps  f0, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [src] "r"(src)
        : "f0", "memory");
}

static inline __attribute__((always_inline))
void vec_xor_u32x8(uint32_t dst[8], const uint32_t src[8])
{
    __asm__ __volatile__(
        "flw.ps  f0, 0(%[dst])\n"
        "flw.ps  f1, 0(%[src])\n"
        "fxor.pi f0, f0, f1\n"
        "fsw.ps  f0, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [src] "r"(src)
        : "f0", "f1", "memory");
}

static inline __attribute__((always_inline))
void vec_add_u32x8(uint32_t dst[8], const uint32_t src[8])
{
    __asm__ __volatile__(
        "flw.ps  f0, 0(%[dst])\n"
        "flw.ps  f1, 0(%[src])\n"
        "fadd.pi f0, f0, f1\n"
        "fsw.ps  f0, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [src] "r"(src)
        : "f0", "f1", "memory");
}

static inline __attribute__((always_inline))
void vec_gather_xor_u32x8(uint32_t dst[8], const uint8_t *base,
                          const uint32_t offsets[8])
{
    __asm__ __volatile__(
        "flw.ps   f31, 0(%[offs])\n"
        "fgw.ps   f0, f31(%[base])\n"
        "flw.ps   f1, 0(%[dst])\n"
        "fxor.pi  f0, f0, f1\n"
        "fsw.ps   f0, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [base] "r"(base), [offs] "r"(offsets)
        : "f0", "f1", "f31", "memory");
}

static inline void vec_set1_u32x8(uint32_t dst[8], uint32_t value)
{
    uint32_t lane;

    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane)
        dst[lane] = value;
}

#define LTFARM_STR2(x) #x
#define LTFARM_STR(x) LTFARM_STR2(x)

#define LTFARM_DEFINE_VEC_BINOP(name, insn)                                        \
static inline __attribute__((always_inline))                                       \
void name(uint32_t dst[8], const uint32_t a[8], const uint32_t b[8])              \
{                                                                                  \
    __asm__ __volatile__(                                                          \
        "flw.ps  f0, 0(%[a])\n"                                                    \
        "flw.ps  f1, 0(%[b])\n"                                                    \
        insn " f0, f0, f1\n"                                                       \
        "fsw.ps  f0, 0(%[dst])\n"                                                  \
        :                                                                          \
        : [dst] "r"(dst), [a] "r"(a), [b] "r"(b)                                  \
        : "f0", "f1", "memory");                                                  \
}

LTFARM_DEFINE_VEC_BINOP(vec_add2_u32x8, "fadd.pi")
LTFARM_DEFINE_VEC_BINOP(vec_xor2_u32x8, "fxor.pi")
LTFARM_DEFINE_VEC_BINOP(vec_and2_u32x8, "fand.pi")
LTFARM_DEFINE_VEC_BINOP(vec_or2_u32x8, "for.pi")

static inline __attribute__((always_inline))
void vec_add4_u32x8(uint32_t dst[8],
                    const uint32_t a[8],
                    const uint32_t b[8],
                    const uint32_t c[8],
                    const uint32_t d[8])
{
    __asm__ __volatile__(
        "flw.ps  f0, 0(%[a])\n"
        "flw.ps  f1, 0(%[b])\n"
        "fadd.pi f0, f0, f1\n"
        "flw.ps  f1, 0(%[c])\n"
        "fadd.pi f0, f0, f1\n"
        "flw.ps  f1, 0(%[d])\n"
        "fadd.pi f0, f0, f1\n"
        "fsw.ps  f0, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [a] "r"(a), [b] "r"(b), [c] "r"(c), [d] "r"(d)
        : "f0", "f1", "memory");
}

static inline __attribute__((always_inline))
void vec_add5_u32x8(uint32_t dst[8],
                    const uint32_t a[8],
                    const uint32_t b[8],
                    const uint32_t c[8],
                    const uint32_t d[8],
                    const uint32_t e[8])
{
    __asm__ __volatile__(
        "flw.ps  f0, 0(%[a])\n"
        "flw.ps  f1, 0(%[b])\n"
        "fadd.pi f0, f0, f1\n"
        "flw.ps  f1, 0(%[c])\n"
        "fadd.pi f0, f0, f1\n"
        "flw.ps  f1, 0(%[d])\n"
        "fadd.pi f0, f0, f1\n"
        "flw.ps  f1, 0(%[e])\n"
        "fadd.pi f0, f0, f1\n"
        "fsw.ps  f0, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [a] "r"(a), [b] "r"(b), [c] "r"(c), [d] "r"(d), [e] "r"(e)
        : "f0", "f1", "memory");
}

#define LTFARM_DEFINE_VEC_SHIFT_RIGHT(name, sh)                                    \
static inline __attribute__((always_inline))                                       \
void name(uint32_t dst[8], const uint32_t src[8])                                 \
{                                                                                  \
    __asm__ __volatile__(                                                          \
        "flw.ps   f0, 0(%[src])\n"                                                 \
        "fsrli.pi f0, f0, " LTFARM_STR(sh) "\n"                                    \
        "fsw.ps   f0, 0(%[dst])\n"                                                 \
        :                                                                          \
        : [dst] "r"(dst), [src] "r"(src)                                          \
        : "f0", "memory");                                                        \
}

#define LTFARM_DEFINE_VEC_ROTR(name, shr, shl)                                     \
static inline __attribute__((always_inline))                                       \
void name(uint32_t dst[8], const uint32_t src[8])                                 \
{                                                                                  \
    __asm__ __volatile__(                                                          \
        "flw.ps   f0, 0(%[src])\n"                                                 \
        "fsrli.pi f1, f0, " LTFARM_STR(shr) "\n"                                   \
        "fslli.pi f0, f0, " LTFARM_STR(shl) "\n"                                   \
        "for.pi   f0, f0, f1\n"                                                    \
        "fsw.ps   f0, 0(%[dst])\n"                                                 \
        :                                                                          \
        : [dst] "r"(dst), [src] "r"(src)                                          \
        : "f0", "f1", "memory");                                                  \
}

LTFARM_DEFINE_VEC_SHIFT_RIGHT(vec_shr3_u32x8, 3)
LTFARM_DEFINE_VEC_SHIFT_RIGHT(vec_shr10_u32x8, 10)
LTFARM_DEFINE_VEC_ROTR(vec_rotr2_u32x8, 2, 30)
LTFARM_DEFINE_VEC_ROTR(vec_rotr6_u32x8, 6, 26)
LTFARM_DEFINE_VEC_ROTR(vec_rotr7_u32x8, 7, 25)
LTFARM_DEFINE_VEC_ROTR(vec_rotr11_u32x8, 11, 21)
LTFARM_DEFINE_VEC_ROTR(vec_rotr13_u32x8, 13, 19)
LTFARM_DEFINE_VEC_ROTR(vec_rotr17_u32x8, 17, 15)
LTFARM_DEFINE_VEC_ROTR(vec_rotr18_u32x8, 18, 14)
LTFARM_DEFINE_VEC_ROTR(vec_rotr19_u32x8, 19, 13)
LTFARM_DEFINE_VEC_ROTR(vec_rotr22_u32x8, 22, 10)
LTFARM_DEFINE_VEC_ROTR(vec_rotr25_u32x8, 25, 7)

static inline void vec_sigma0_u32x8(uint32_t dst[8], const uint32_t src[8])
{
    __asm__ __volatile__(
        "flw.ps   f0, 0(%[src])\n"
        "fsrli.pi f1, f0, 7\n"
        "fslli.pi f2, f0, 25\n"
        "for.pi   f1, f1, f2\n"
        "fsrli.pi f2, f0, 18\n"
        "fslli.pi f3, f0, 14\n"
        "for.pi   f2, f2, f3\n"
        "fxor.pi  f1, f1, f2\n"
        "fsrli.pi f2, f0, 3\n"
        "fxor.pi  f1, f1, f2\n"
        "fsw.ps   f1, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [src] "r"(src)
        : "f0", "f1", "f2", "f3", "memory");
}

static inline void vec_sigma1_u32x8(uint32_t dst[8], const uint32_t src[8])
{
    __asm__ __volatile__(
        "flw.ps   f0, 0(%[src])\n"
        "fsrli.pi f1, f0, 17\n"
        "fslli.pi f2, f0, 15\n"
        "for.pi   f1, f1, f2\n"
        "fsrli.pi f2, f0, 19\n"
        "fslli.pi f3, f0, 13\n"
        "for.pi   f2, f2, f3\n"
        "fxor.pi  f1, f1, f2\n"
        "fsrli.pi f2, f0, 10\n"
        "fxor.pi  f1, f1, f2\n"
        "fsw.ps   f1, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [src] "r"(src)
        : "f0", "f1", "f2", "f3", "memory");
}

static inline void vec_bsig0_u32x8(uint32_t dst[8], const uint32_t src[8])
{
    __asm__ __volatile__(
        "flw.ps   f0, 0(%[src])\n"
        "fsrli.pi f1, f0, 2\n"
        "fslli.pi f2, f0, 30\n"
        "for.pi   f1, f1, f2\n"
        "fsrli.pi f2, f0, 13\n"
        "fslli.pi f3, f0, 19\n"
        "for.pi   f2, f2, f3\n"
        "fxor.pi  f1, f1, f2\n"
        "fsrli.pi f2, f0, 22\n"
        "fslli.pi f3, f0, 10\n"
        "for.pi   f2, f2, f3\n"
        "fxor.pi  f1, f1, f2\n"
        "fsw.ps   f1, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [src] "r"(src)
        : "f0", "f1", "f2", "f3", "memory");
}

static inline void vec_bsig1_u32x8(uint32_t dst[8], const uint32_t src[8])
{
    __asm__ __volatile__(
        "flw.ps   f0, 0(%[src])\n"
        "fsrli.pi f1, f0, 6\n"
        "fslli.pi f2, f0, 26\n"
        "for.pi   f1, f1, f2\n"
        "fsrli.pi f2, f0, 11\n"
        "fslli.pi f3, f0, 21\n"
        "for.pi   f2, f2, f3\n"
        "fxor.pi  f1, f1, f2\n"
        "fsrli.pi f2, f0, 25\n"
        "fslli.pi f3, f0, 7\n"
        "for.pi   f2, f2, f3\n"
        "fxor.pi  f1, f1, f2\n"
        "fsw.ps   f1, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [src] "r"(src)
        : "f0", "f1", "f2", "f3", "memory");
}

static inline void vec_ch_u32x8(uint32_t dst[8],
                                const uint32_t x[8],
                                const uint32_t y[8],
                                const uint32_t z[8])
{
    __asm__ __volatile__(
        "flw.ps  f0, 0(%[x])\n"
        "flw.ps  f1, 0(%[y])\n"
        "flw.ps  f2, 0(%[z])\n"
        "fxor.pi f1, f1, f2\n"
        "fand.pi f1, f1, f0\n"
        "fxor.pi f1, f1, f2\n"
        "fsw.ps  f1, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [x] "r"(x), [y] "r"(y), [z] "r"(z)
        : "f0", "f1", "f2", "memory");
}

static inline void vec_maj_u32x8(uint32_t dst[8],
                                 const uint32_t x[8],
                                 const uint32_t y[8],
                                 const uint32_t z[8])
{
    __asm__ __volatile__(
        "flw.ps  f0, 0(%[x])\n"
        "flw.ps  f1, 0(%[y])\n"
        "flw.ps  f2, 0(%[z])\n"
        "for.pi  f3, f0, f1\n"
        "fand.pi f3, f3, f2\n"
        "fand.pi f0, f0, f1\n"
        "for.pi  f0, f0, f3\n"
        "fsw.ps  f0, 0(%[dst])\n"
        :
        : [dst] "r"(dst), [x] "r"(x), [y] "r"(y), [z] "r"(z)
        : "f0", "f1", "f2", "f3", "memory");
}

#define LTFARM_DEFINE_VEC_XOR_ROTL_ADD(name, shl, shr)                            \
static inline __attribute__((always_inline))                                      \
void name(uint32_t dst[8], const uint32_t a[8], const uint32_t b[8])             \
{                                                                                 \
    __asm__ __volatile__(                                                         \
        "flw.ps   f0, 0(%[a])\n"                                                  \
        "flw.ps   f1, 0(%[b])\n"                                                  \
        "fadd.pi  f0, f0, f1\n"                                                   \
        "fslli.pi f1, f0, " LTFARM_STR(shl) "\n"                                  \
        "fsrli.pi f0, f0, " LTFARM_STR(shr) "\n"                                  \
        "for.pi   f0, f0, f1\n"                                                   \
        "flw.ps   f1, 0(%[dst])\n"                                                \
        "fxor.pi  f0, f0, f1\n"                                                   \
        "fsw.ps   f0, 0(%[dst])\n"                                                \
        :                                                                         \
        : [dst] "r"(dst), [a] "r"(a), [b] "r"(b)                                 \
        : "f0", "f1", "memory");                                                 \
}

LTFARM_DEFINE_VEC_XOR_ROTL_ADD(vec_xor_rotl_add_7, 7, 25)
LTFARM_DEFINE_VEC_XOR_ROTL_ADD(vec_xor_rotl_add_9, 9, 23)
LTFARM_DEFINE_VEC_XOR_ROTL_ADD(vec_xor_rotl_add_13, 13, 19)
LTFARM_DEFINE_VEC_XOR_ROTL_ADD(vec_xor_rotl_add_18, 18, 14)

#define LTFARM_SALSA_STEP(DST, A, B, SHL, SHR)                                    \
        "fadd.pi  f16, " A ", " B "\n"                                            \
        "fslli.pi f17, f16, " LTFARM_STR(SHL) "\n"                                \
        "fsrli.pi f16, f16, " LTFARM_STR(SHR) "\n"                                \
        "for.pi   f16, f16, f17\n"                                                \
        "fxor.pi  " DST ", " DST ", f16\n"

static inline void vec_copy_state_u32x8(uint32_t dst[8][8], const uint32_t src[8][8])
{
    uint32_t i;

    for (i = 0u; i < 8u; ++i)
        vec_copy_u32x8(dst[i], src[i]);
}

#define LTFARM_SHA_UPDATE_W(DST, WM2, WM7, WM15)                                  \
        "fsrli.pi f24, " WM2 ", 17\n"                                             \
        "fslli.pi f25, " WM2 ", 15\n"                                             \
        "for.pi   f24, f24, f25\n"                                                \
        "fsrli.pi f25, " WM2 ", 19\n"                                             \
        "fslli.pi f26, " WM2 ", 13\n"                                             \
        "for.pi   f25, f25, f26\n"                                                \
        "fxor.pi  f24, f24, f25\n"                                                \
        "fsrli.pi f25, " WM2 ", 10\n"                                             \
        "fxor.pi  f24, f24, f25\n"                                                \
        "fsrli.pi f25, " WM15 ", 7\n"                                             \
        "fslli.pi f26, " WM15 ", 25\n"                                            \
        "for.pi   f25, f25, f26\n"                                                \
        "fsrli.pi f26, " WM15 ", 18\n"                                            \
        "fslli.pi f27, " WM15 ", 14\n"                                            \
        "for.pi   f26, f26, f27\n"                                                \
        "fxor.pi  f25, f25, f26\n"                                                \
        "fsrli.pi f26, " WM15 ", 3\n"                                             \
        "fxor.pi  f25, f25, f26\n"                                                \
        "fadd.pi  f24, f24, " WM7 "\n"                                            \
        "fadd.pi  f24, f24, f25\n"                                                \
        "fadd.pi  " DST ", " DST ", f24\n"

#define LTFARM_SHA_ROUND(KOFF, W, A, B, C, D, E, F, G, H)                         \
        "flw.ps   f24, " LTFARM_STR(KOFF) "(%[kbase])\n"                          \
        "fadd.pi  f24, f24, " W "\n"                                              \
        "fsrli.pi f25, " E ", 6\n"                                                \
        "fslli.pi f26, " E ", 26\n"                                               \
        "for.pi   f25, f25, f26\n"                                                \
        "fsrli.pi f26, " E ", 11\n"                                               \
        "fslli.pi f27, " E ", 21\n"                                               \
        "for.pi   f26, f26, f27\n"                                                \
        "fxor.pi  f25, f25, f26\n"                                                \
        "fsrli.pi f26, " E ", 25\n"                                               \
        "fslli.pi f27, " E ", 7\n"                                                \
        "for.pi   f26, f26, f27\n"                                                \
        "fxor.pi  f25, f25, f26\n"                                                \
        "fxor.pi  f26, " F ", " G "\n"                                            \
        "fand.pi  f26, f26, " E "\n"                                              \
        "fxor.pi  f26, f26, " G "\n"                                              \
        "fadd.pi  f24, f24, f25\n"                                                \
        "fadd.pi  f24, f24, f26\n"                                                \
        "fadd.pi  f24, f24, " H "\n"                                              \
        "fsrli.pi f25, " A ", 2\n"                                                \
        "fslli.pi f26, " A ", 30\n"                                               \
        "for.pi   f25, f25, f26\n"                                                \
        "fsrli.pi f26, " A ", 13\n"                                               \
        "fslli.pi f27, " A ", 19\n"                                               \
        "for.pi   f26, f26, f27\n"                                                \
        "fxor.pi  f25, f25, f26\n"                                                \
        "fsrli.pi f26, " A ", 22\n"                                               \
        "fslli.pi f27, " A ", 10\n"                                               \
        "for.pi   f26, f26, f27\n"                                                \
        "fxor.pi  f25, f25, f26\n"                                                \
        "for.pi   f26, " A ", " B "\n"                                            \
        "fand.pi  f26, f26, " C "\n"                                              \
        "fand.pi  f27, " A ", " B "\n"                                            \
        "for.pi   f26, f26, f27\n"                                                \
        "fadd.pi  f25, f25, f26\n"                                                \
        "fadd.pi  " D ", " D ", f24\n"                                            \
        "fadd.pi  " H ", f24, f25\n"

static void sha256_init_state_batch(uint32_t state[8][LTFARM_SCRYPT_LANES])
{
    uint32_t i;

    for (i = 0u; i < 8u; ++i)
        vec_set1_u32x8(state[i], kSha256Init[i]);
}

static void sha256_store_digest_batch(
    const uint32_t state[8][LTFARM_SCRYPT_LANES],
    uint8_t digest[LTFARM_SCRYPT_LANES][32])
{
    uint32_t lane;
    uint32_t i;

    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        for (i = 0u; i < 8u; ++i)
            be32enc(&digest[lane][i * 4u], state[i][lane]);
    }
}

static void sha256_transform_batch(
    uint32_t state[8][LTFARM_SCRYPT_LANES],
    const uint8_t block[LTFARM_SCRYPT_LANES][64])
{
    uint32_t w[16][LTFARM_SCRYPT_LANES] __attribute__((aligned(32)));
    uint32_t i;
    uint32_t lane;

    vec_enable_all_lanes();

    for (i = 0u; i < 16u; ++i) {
        for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane)
            w[i][lane] = be32dec(&block[lane][i * 4u]);
    }
    __asm__ __volatile__(
        "flw.ps f0, 0(%[s0])\n"
        "flw.ps f1, 0(%[s1])\n"
        "flw.ps f2, 0(%[s2])\n"
        "flw.ps f3, 0(%[s3])\n"
        "flw.ps f4, 0(%[s4])\n"
        "flw.ps f5, 0(%[s5])\n"
        "flw.ps f6, 0(%[s6])\n"
        "flw.ps f7, 0(%[s7])\n"
        "flw.ps f8, 0(%[w0])\n"
        "flw.ps f9, 0(%[w1])\n"
        "flw.ps f10, 0(%[w2])\n"
        "flw.ps f11, 0(%[w3])\n"
        "flw.ps f12, 0(%[w4])\n"
        "flw.ps f13, 0(%[w5])\n"
        "flw.ps f14, 0(%[w6])\n"
        "flw.ps f15, 0(%[w7])\n"
        "flw.ps f16, 0(%[w8])\n"
        "flw.ps f17, 0(%[w9])\n"
        "flw.ps f18, 0(%[w10])\n"
        "flw.ps f19, 0(%[w11])\n"
        "flw.ps f20, 0(%[w12])\n"
        "flw.ps f21, 0(%[w13])\n"
        "flw.ps f22, 0(%[w14])\n"
        "flw.ps f23, 0(%[w15])\n"
        LTFARM_SHA_ROUND(0, "f8", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7")
        LTFARM_SHA_ROUND(32, "f9", "f7", "f0", "f1", "f2", "f3", "f4", "f5", "f6")
        LTFARM_SHA_ROUND(64, "f10", "f6", "f7", "f0", "f1", "f2", "f3", "f4", "f5")
        LTFARM_SHA_ROUND(96, "f11", "f5", "f6", "f7", "f0", "f1", "f2", "f3", "f4")
        LTFARM_SHA_ROUND(128, "f12", "f4", "f5", "f6", "f7", "f0", "f1", "f2", "f3")
        LTFARM_SHA_ROUND(160, "f13", "f3", "f4", "f5", "f6", "f7", "f0", "f1", "f2")
        LTFARM_SHA_ROUND(192, "f14", "f2", "f3", "f4", "f5", "f6", "f7", "f0", "f1")
        LTFARM_SHA_ROUND(224, "f15", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f0")
        LTFARM_SHA_ROUND(256, "f16", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7")
        LTFARM_SHA_ROUND(288, "f17", "f7", "f0", "f1", "f2", "f3", "f4", "f5", "f6")
        LTFARM_SHA_ROUND(320, "f18", "f6", "f7", "f0", "f1", "f2", "f3", "f4", "f5")
        LTFARM_SHA_ROUND(352, "f19", "f5", "f6", "f7", "f0", "f1", "f2", "f3", "f4")
        LTFARM_SHA_ROUND(384, "f20", "f4", "f5", "f6", "f7", "f0", "f1", "f2", "f3")
        LTFARM_SHA_ROUND(416, "f21", "f3", "f4", "f5", "f6", "f7", "f0", "f1", "f2")
        LTFARM_SHA_ROUND(448, "f22", "f2", "f3", "f4", "f5", "f6", "f7", "f0", "f1")
        LTFARM_SHA_ROUND(480, "f23", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f0")
        LTFARM_SHA_UPDATE_W("f8", "f22", "f17", "f9")
        LTFARM_SHA_ROUND(512, "f8", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7")
        LTFARM_SHA_UPDATE_W("f9", "f23", "f18", "f10")
        LTFARM_SHA_ROUND(544, "f9", "f7", "f0", "f1", "f2", "f3", "f4", "f5", "f6")
        LTFARM_SHA_UPDATE_W("f10", "f8", "f19", "f11")
        LTFARM_SHA_ROUND(576, "f10", "f6", "f7", "f0", "f1", "f2", "f3", "f4", "f5")
        LTFARM_SHA_UPDATE_W("f11", "f9", "f20", "f12")
        LTFARM_SHA_ROUND(608, "f11", "f5", "f6", "f7", "f0", "f1", "f2", "f3", "f4")
        LTFARM_SHA_UPDATE_W("f12", "f10", "f21", "f13")
        LTFARM_SHA_ROUND(640, "f12", "f4", "f5", "f6", "f7", "f0", "f1", "f2", "f3")
        LTFARM_SHA_UPDATE_W("f13", "f11", "f22", "f14")
        LTFARM_SHA_ROUND(672, "f13", "f3", "f4", "f5", "f6", "f7", "f0", "f1", "f2")
        LTFARM_SHA_UPDATE_W("f14", "f12", "f23", "f15")
        LTFARM_SHA_ROUND(704, "f14", "f2", "f3", "f4", "f5", "f6", "f7", "f0", "f1")
        LTFARM_SHA_UPDATE_W("f15", "f13", "f8", "f16")
        LTFARM_SHA_ROUND(736, "f15", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f0")
        LTFARM_SHA_UPDATE_W("f16", "f14", "f9", "f17")
        LTFARM_SHA_ROUND(768, "f16", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7")
        LTFARM_SHA_UPDATE_W("f17", "f15", "f10", "f18")
        LTFARM_SHA_ROUND(800, "f17", "f7", "f0", "f1", "f2", "f3", "f4", "f5", "f6")
        LTFARM_SHA_UPDATE_W("f18", "f16", "f11", "f19")
        LTFARM_SHA_ROUND(832, "f18", "f6", "f7", "f0", "f1", "f2", "f3", "f4", "f5")
        LTFARM_SHA_UPDATE_W("f19", "f17", "f12", "f20")
        LTFARM_SHA_ROUND(864, "f19", "f5", "f6", "f7", "f0", "f1", "f2", "f3", "f4")
        LTFARM_SHA_UPDATE_W("f20", "f18", "f13", "f21")
        LTFARM_SHA_ROUND(896, "f20", "f4", "f5", "f6", "f7", "f0", "f1", "f2", "f3")
        LTFARM_SHA_UPDATE_W("f21", "f19", "f14", "f22")
        LTFARM_SHA_ROUND(928, "f21", "f3", "f4", "f5", "f6", "f7", "f0", "f1", "f2")
        LTFARM_SHA_UPDATE_W("f22", "f20", "f15", "f23")
        LTFARM_SHA_ROUND(960, "f22", "f2", "f3", "f4", "f5", "f6", "f7", "f0", "f1")
        LTFARM_SHA_UPDATE_W("f23", "f21", "f16", "f8")
        LTFARM_SHA_ROUND(992, "f23", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f0")
        LTFARM_SHA_UPDATE_W("f8", "f22", "f17", "f9")
        LTFARM_SHA_ROUND(1024, "f8", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7")
        LTFARM_SHA_UPDATE_W("f9", "f23", "f18", "f10")
        LTFARM_SHA_ROUND(1056, "f9", "f7", "f0", "f1", "f2", "f3", "f4", "f5", "f6")
        LTFARM_SHA_UPDATE_W("f10", "f8", "f19", "f11")
        LTFARM_SHA_ROUND(1088, "f10", "f6", "f7", "f0", "f1", "f2", "f3", "f4", "f5")
        LTFARM_SHA_UPDATE_W("f11", "f9", "f20", "f12")
        LTFARM_SHA_ROUND(1120, "f11", "f5", "f6", "f7", "f0", "f1", "f2", "f3", "f4")
        LTFARM_SHA_UPDATE_W("f12", "f10", "f21", "f13")
        LTFARM_SHA_ROUND(1152, "f12", "f4", "f5", "f6", "f7", "f0", "f1", "f2", "f3")
        LTFARM_SHA_UPDATE_W("f13", "f11", "f22", "f14")
        LTFARM_SHA_ROUND(1184, "f13", "f3", "f4", "f5", "f6", "f7", "f0", "f1", "f2")
        LTFARM_SHA_UPDATE_W("f14", "f12", "f23", "f15")
        LTFARM_SHA_ROUND(1216, "f14", "f2", "f3", "f4", "f5", "f6", "f7", "f0", "f1")
        LTFARM_SHA_UPDATE_W("f15", "f13", "f8", "f16")
        LTFARM_SHA_ROUND(1248, "f15", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f0")
        LTFARM_SHA_UPDATE_W("f16", "f14", "f9", "f17")
        LTFARM_SHA_ROUND(1280, "f16", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7")
        LTFARM_SHA_UPDATE_W("f17", "f15", "f10", "f18")
        LTFARM_SHA_ROUND(1312, "f17", "f7", "f0", "f1", "f2", "f3", "f4", "f5", "f6")
        LTFARM_SHA_UPDATE_W("f18", "f16", "f11", "f19")
        LTFARM_SHA_ROUND(1344, "f18", "f6", "f7", "f0", "f1", "f2", "f3", "f4", "f5")
        LTFARM_SHA_UPDATE_W("f19", "f17", "f12", "f20")
        LTFARM_SHA_ROUND(1376, "f19", "f5", "f6", "f7", "f0", "f1", "f2", "f3", "f4")
        LTFARM_SHA_UPDATE_W("f20", "f18", "f13", "f21")
        LTFARM_SHA_ROUND(1408, "f20", "f4", "f5", "f6", "f7", "f0", "f1", "f2", "f3")
        LTFARM_SHA_UPDATE_W("f21", "f19", "f14", "f22")
        LTFARM_SHA_ROUND(1440, "f21", "f3", "f4", "f5", "f6", "f7", "f0", "f1", "f2")
        LTFARM_SHA_UPDATE_W("f22", "f20", "f15", "f23")
        LTFARM_SHA_ROUND(1472, "f22", "f2", "f3", "f4", "f5", "f6", "f7", "f0", "f1")
        LTFARM_SHA_UPDATE_W("f23", "f21", "f16", "f8")
        LTFARM_SHA_ROUND(1504, "f23", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f0")
        LTFARM_SHA_UPDATE_W("f8", "f22", "f17", "f9")
        LTFARM_SHA_ROUND(1536, "f8", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7")
        LTFARM_SHA_UPDATE_W("f9", "f23", "f18", "f10")
        LTFARM_SHA_ROUND(1568, "f9", "f7", "f0", "f1", "f2", "f3", "f4", "f5", "f6")
        LTFARM_SHA_UPDATE_W("f10", "f8", "f19", "f11")
        LTFARM_SHA_ROUND(1600, "f10", "f6", "f7", "f0", "f1", "f2", "f3", "f4", "f5")
        LTFARM_SHA_UPDATE_W("f11", "f9", "f20", "f12")
        LTFARM_SHA_ROUND(1632, "f11", "f5", "f6", "f7", "f0", "f1", "f2", "f3", "f4")
        LTFARM_SHA_UPDATE_W("f12", "f10", "f21", "f13")
        LTFARM_SHA_ROUND(1664, "f12", "f4", "f5", "f6", "f7", "f0", "f1", "f2", "f3")
        LTFARM_SHA_UPDATE_W("f13", "f11", "f22", "f14")
        LTFARM_SHA_ROUND(1696, "f13", "f3", "f4", "f5", "f6", "f7", "f0", "f1", "f2")
        LTFARM_SHA_UPDATE_W("f14", "f12", "f23", "f15")
        LTFARM_SHA_ROUND(1728, "f14", "f2", "f3", "f4", "f5", "f6", "f7", "f0", "f1")
        LTFARM_SHA_UPDATE_W("f15", "f13", "f8", "f16")
        LTFARM_SHA_ROUND(1760, "f15", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f0")
        LTFARM_SHA_UPDATE_W("f16", "f14", "f9", "f17")
        LTFARM_SHA_ROUND(1792, "f16", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7")
        LTFARM_SHA_UPDATE_W("f17", "f15", "f10", "f18")
        LTFARM_SHA_ROUND(1824, "f17", "f7", "f0", "f1", "f2", "f3", "f4", "f5", "f6")
        LTFARM_SHA_UPDATE_W("f18", "f16", "f11", "f19")
        LTFARM_SHA_ROUND(1856, "f18", "f6", "f7", "f0", "f1", "f2", "f3", "f4", "f5")
        LTFARM_SHA_UPDATE_W("f19", "f17", "f12", "f20")
        LTFARM_SHA_ROUND(1888, "f19", "f5", "f6", "f7", "f0", "f1", "f2", "f3", "f4")
        LTFARM_SHA_UPDATE_W("f20", "f18", "f13", "f21")
        LTFARM_SHA_ROUND(1920, "f20", "f4", "f5", "f6", "f7", "f0", "f1", "f2", "f3")
        LTFARM_SHA_UPDATE_W("f21", "f19", "f14", "f22")
        LTFARM_SHA_ROUND(1952, "f21", "f3", "f4", "f5", "f6", "f7", "f0", "f1", "f2")
        LTFARM_SHA_UPDATE_W("f22", "f20", "f15", "f23")
        LTFARM_SHA_ROUND(1984, "f22", "f2", "f3", "f4", "f5", "f6", "f7", "f0", "f1")
        LTFARM_SHA_UPDATE_W("f23", "f21", "f16", "f8")
        LTFARM_SHA_ROUND(2016, "f23", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f0")
        "flw.ps f24, 0(%[s0])\n"
        "fadd.pi f0, f0, f24\n"
        "fsw.ps f0, 0(%[s0])\n"
        "flw.ps f24, 0(%[s1])\n"
        "fadd.pi f1, f1, f24\n"
        "fsw.ps f1, 0(%[s1])\n"
        "flw.ps f24, 0(%[s2])\n"
        "fadd.pi f2, f2, f24\n"
        "fsw.ps f2, 0(%[s2])\n"
        "flw.ps f24, 0(%[s3])\n"
        "fadd.pi f3, f3, f24\n"
        "fsw.ps f3, 0(%[s3])\n"
        "flw.ps f24, 0(%[s4])\n"
        "fadd.pi f4, f4, f24\n"
        "fsw.ps f4, 0(%[s4])\n"
        "flw.ps f24, 0(%[s5])\n"
        "fadd.pi f5, f5, f24\n"
        "fsw.ps f5, 0(%[s5])\n"
        "flw.ps f24, 0(%[s6])\n"
        "fadd.pi f6, f6, f24\n"
        "fsw.ps f6, 0(%[s6])\n"
        "flw.ps f24, 0(%[s7])\n"
        "fadd.pi f7, f7, f24\n"
        "fsw.ps f7, 0(%[s7])\n"
        :
        : [kbase] "r"(&kSha256KVec[0][0]),
          [s0] "r"(state[0]), [s1] "r"(state[1]), [s2] "r"(state[2]), [s3] "r"(state[3]),
          [s4] "r"(state[4]), [s5] "r"(state[5]), [s6] "r"(state[6]), [s7] "r"(state[7]),
          [w0] "r"(w[0]), [w1] "r"(w[1]), [w2] "r"(w[2]), [w3] "r"(w[3]),
          [w4] "r"(w[4]), [w5] "r"(w[5]), [w6] "r"(w[6]), [w7] "r"(w[7]),
          [w8] "r"(w[8]), [w9] "r"(w[9]), [w10] "r"(w[10]), [w11] "r"(w[11]),
          [w12] "r"(w[12]), [w13] "r"(w[13]), [w14] "r"(w[14]), [w15] "r"(w[15])
        : "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
          "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
          "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
          "f24", "f25", "f26", "f27", "memory");
}

static void sha256_80_batch(const uint8_t input[LTFARM_SCRYPT_LANES][80],
                            uint8_t digest[LTFARM_SCRYPT_LANES][32])
{
    uint32_t state[8][LTFARM_SCRYPT_LANES] __attribute__((aligned(32)));
    uint8_t block0[LTFARM_SCRYPT_LANES][64];
    uint8_t block1[LTFARM_SCRYPT_LANES][64];
    uint32_t lane;

    memset(block1, 0, sizeof(block1));
    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        memcpy(block0[lane], input[lane], 64u);
        memcpy(block1[lane], &input[lane][64], 16u);
        block1[lane][16] = 0x80u;
        block1[lane][62] = 0x02u;
        block1[lane][63] = 0x80u;
    }

    sha256_init_state_batch(state);
    sha256_transform_batch(state, block0);
    sha256_transform_batch(state, block1);
    sha256_store_digest_batch(state, digest);
}

static void hmac_sha256_prepare_bases_batch(
    const uint8_t key[LTFARM_SCRYPT_LANES][LTFARM_SCRYPT_INPUT_BYTES],
    uint32_t inner_base[8][LTFARM_SCRYPT_LANES],
    uint32_t outer_base[8][LTFARM_SCRYPT_LANES])
{
    uint8_t khash[LTFARM_SCRYPT_LANES][32];
    uint8_t block[LTFARM_SCRYPT_LANES][64];
    uint32_t lane;
    uint32_t i;

    sha256_80_batch(key, khash);

    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        memset(block[lane], 0x36, sizeof(block[lane]));
        for (i = 0u; i < 32u; ++i)
            block[lane][i] ^= khash[lane][i];
    }
    sha256_init_state_batch(inner_base);
    sha256_transform_batch(inner_base, block);

    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        memset(block[lane], 0x5c, sizeof(block[lane]));
        for (i = 0u; i < 32u; ++i)
            block[lane][i] ^= khash[lane][i];
    }
    sha256_init_state_batch(outer_base);
    sha256_transform_batch(outer_base, block);
}

static void hmac_sha256_84_x4_batch(
    const uint32_t inner_base[8][LTFARM_SCRYPT_LANES],
    const uint32_t outer_base[8][LTFARM_SCRYPT_LANES],
    const uint8_t salt[LTFARM_SCRYPT_LANES][80],
    uint8_t out[LTFARM_SCRYPT_LANES][128])
{
    uint32_t state[8][LTFARM_SCRYPT_LANES] __attribute__((aligned(32)));
    uint8_t block0[LTFARM_SCRYPT_LANES][64];
    uint8_t block1[LTFARM_SCRYPT_LANES][64];
    uint8_t outer_block[LTFARM_SCRYPT_LANES][64];
    uint8_t inner_digest[LTFARM_SCRYPT_LANES][32];
    uint32_t lane;
    uint32_t blk;

    for (blk = 1u; blk <= 4u; ++blk) {
        for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane)
            memcpy(block0[lane], salt[lane], 64u);

        memset(block1, 0, sizeof(block1));
        for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
            memcpy(block1[lane], &salt[lane][64], 16u);
            be32enc(&block1[lane][16], blk);
            block1[lane][20] = 0x80u;
            block1[lane][62] = 0x04u;
            block1[lane][63] = 0xa0u;
        }

        vec_copy_state_u32x8(state, inner_base);
        sha256_transform_batch(state, block0);
        sha256_transform_batch(state, block1);
        sha256_store_digest_batch(state, inner_digest);

        memset(outer_block, 0, sizeof(outer_block));
        for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
            memcpy(outer_block[lane], inner_digest[lane], 32u);
            outer_block[lane][32] = 0x80u;
            outer_block[lane][62] = 0x03u;
            outer_block[lane][63] = 0x00u;
        }

        vec_copy_state_u32x8(state, outer_base);
        sha256_transform_batch(state, outer_block);
        sha256_store_digest_batch(state, inner_digest);

        for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane)
            memcpy(&out[lane][(blk - 1u) * 32u], inner_digest[lane], 32u);
    }
}

static void hmac_sha256_132_x1_batch(
    const uint32_t inner_base[8][LTFARM_SCRYPT_LANES],
    const uint32_t outer_base[8][LTFARM_SCRYPT_LANES],
    const uint8_t salt[LTFARM_SCRYPT_LANES][128],
    uint8_t out[LTFARM_SCRYPT_LANES][32])
{
    uint32_t state[8][LTFARM_SCRYPT_LANES] __attribute__((aligned(32)));
    uint8_t block0[LTFARM_SCRYPT_LANES][64];
    uint8_t block1[LTFARM_SCRYPT_LANES][64];
    uint8_t block2[LTFARM_SCRYPT_LANES][64];
    uint8_t outer_block[LTFARM_SCRYPT_LANES][64];
    uint8_t inner_digest[LTFARM_SCRYPT_LANES][32];
    uint32_t lane;

    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        memcpy(block0[lane], &salt[lane][0], 64u);
        memcpy(block1[lane], &salt[lane][64], 64u);
    }

    memset(block2, 0, sizeof(block2));
    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        be32enc(&block2[lane][0], 1u);
        block2[lane][4] = 0x80u;
        block2[lane][62] = 0x06u;
        block2[lane][63] = 0x20u;
    }

    vec_copy_state_u32x8(state, inner_base);
    sha256_transform_batch(state, block0);
    sha256_transform_batch(state, block1);
    sha256_transform_batch(state, block2);
    sha256_store_digest_batch(state, inner_digest);

    memset(outer_block, 0, sizeof(outer_block));
    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        memcpy(outer_block[lane], inner_digest[lane], 32u);
        outer_block[lane][32] = 0x80u;
        outer_block[lane][62] = 0x03u;
        outer_block[lane][63] = 0x00u;
    }

    vec_copy_state_u32x8(state, outer_base);
    sha256_transform_batch(state, outer_block);
    sha256_store_digest_batch(state, out);
}

static inline void xor_salsa8x8(uint32_t (*b)[LTFARM_SCRYPT_LANES],
                                const uint32_t (*bx)[LTFARM_SCRYPT_LANES])
{
    uint32_t x[16][LTFARM_SCRYPT_LANES] __attribute__((aligned(32)));
    uint32_t w;

    for (w = 0u; w < 16u; ++w) {
        vec_xor_u32x8(b[w], bx[w]);
        vec_copy_u32x8(x[w], b[w]);
    }

    __asm__ __volatile__(
        "flw.ps f0, 0(%[x0])\n"
        "flw.ps f1, 0(%[x1])\n"
        "flw.ps f2, 0(%[x2])\n"
        "flw.ps f3, 0(%[x3])\n"
        "flw.ps f4, 0(%[x4])\n"
        "flw.ps f5, 0(%[x5])\n"
        "flw.ps f6, 0(%[x6])\n"
        "flw.ps f7, 0(%[x7])\n"
        "flw.ps f8, 0(%[x8])\n"
        "flw.ps f9, 0(%[x9])\n"
        "flw.ps f10, 0(%[x10])\n"
        "flw.ps f11, 0(%[x11])\n"
        "flw.ps f12, 0(%[x12])\n"
        "flw.ps f13, 0(%[x13])\n"
        "flw.ps f14, 0(%[x14])\n"
        "flw.ps f15, 0(%[x15])\n"
        LTFARM_SALSA_STEP("f4", "f0", "f12", 7, 25)
        LTFARM_SALSA_STEP("f9", "f5", "f1", 7, 25)
        LTFARM_SALSA_STEP("f14", "f10", "f6", 7, 25)
        LTFARM_SALSA_STEP("f3", "f15", "f11", 7, 25)
        LTFARM_SALSA_STEP("f8", "f4", "f0", 9, 23)
        LTFARM_SALSA_STEP("f13", "f9", "f5", 9, 23)
        LTFARM_SALSA_STEP("f2", "f14", "f10", 9, 23)
        LTFARM_SALSA_STEP("f7", "f3", "f15", 9, 23)
        LTFARM_SALSA_STEP("f12", "f8", "f4", 13, 19)
        LTFARM_SALSA_STEP("f1", "f13", "f9", 13, 19)
        LTFARM_SALSA_STEP("f6", "f2", "f14", 13, 19)
        LTFARM_SALSA_STEP("f11", "f7", "f3", 13, 19)
        LTFARM_SALSA_STEP("f0", "f12", "f8", 18, 14)
        LTFARM_SALSA_STEP("f5", "f1", "f13", 18, 14)
        LTFARM_SALSA_STEP("f10", "f6", "f2", 18, 14)
        LTFARM_SALSA_STEP("f15", "f11", "f7", 18, 14)
        LTFARM_SALSA_STEP("f1", "f0", "f3", 7, 25)
        LTFARM_SALSA_STEP("f6", "f5", "f4", 7, 25)
        LTFARM_SALSA_STEP("f11", "f10", "f9", 7, 25)
        LTFARM_SALSA_STEP("f12", "f15", "f14", 7, 25)
        LTFARM_SALSA_STEP("f2", "f1", "f0", 9, 23)
        LTFARM_SALSA_STEP("f7", "f6", "f5", 9, 23)
        LTFARM_SALSA_STEP("f8", "f11", "f10", 9, 23)
        LTFARM_SALSA_STEP("f13", "f12", "f15", 9, 23)
        LTFARM_SALSA_STEP("f3", "f2", "f1", 13, 19)
        LTFARM_SALSA_STEP("f4", "f7", "f6", 13, 19)
        LTFARM_SALSA_STEP("f9", "f8", "f11", 13, 19)
        LTFARM_SALSA_STEP("f14", "f13", "f12", 13, 19)
        LTFARM_SALSA_STEP("f0", "f3", "f2", 18, 14)
        LTFARM_SALSA_STEP("f5", "f4", "f7", 18, 14)
        LTFARM_SALSA_STEP("f10", "f9", "f8", 18, 14)
        LTFARM_SALSA_STEP("f15", "f14", "f13", 18, 14)
        LTFARM_SALSA_STEP("f4", "f0", "f12", 7, 25)
        LTFARM_SALSA_STEP("f9", "f5", "f1", 7, 25)
        LTFARM_SALSA_STEP("f14", "f10", "f6", 7, 25)
        LTFARM_SALSA_STEP("f3", "f15", "f11", 7, 25)
        LTFARM_SALSA_STEP("f8", "f4", "f0", 9, 23)
        LTFARM_SALSA_STEP("f13", "f9", "f5", 9, 23)
        LTFARM_SALSA_STEP("f2", "f14", "f10", 9, 23)
        LTFARM_SALSA_STEP("f7", "f3", "f15", 9, 23)
        LTFARM_SALSA_STEP("f12", "f8", "f4", 13, 19)
        LTFARM_SALSA_STEP("f1", "f13", "f9", 13, 19)
        LTFARM_SALSA_STEP("f6", "f2", "f14", 13, 19)
        LTFARM_SALSA_STEP("f11", "f7", "f3", 13, 19)
        LTFARM_SALSA_STEP("f0", "f12", "f8", 18, 14)
        LTFARM_SALSA_STEP("f5", "f1", "f13", 18, 14)
        LTFARM_SALSA_STEP("f10", "f6", "f2", 18, 14)
        LTFARM_SALSA_STEP("f15", "f11", "f7", 18, 14)
        LTFARM_SALSA_STEP("f1", "f0", "f3", 7, 25)
        LTFARM_SALSA_STEP("f6", "f5", "f4", 7, 25)
        LTFARM_SALSA_STEP("f11", "f10", "f9", 7, 25)
        LTFARM_SALSA_STEP("f12", "f15", "f14", 7, 25)
        LTFARM_SALSA_STEP("f2", "f1", "f0", 9, 23)
        LTFARM_SALSA_STEP("f7", "f6", "f5", 9, 23)
        LTFARM_SALSA_STEP("f8", "f11", "f10", 9, 23)
        LTFARM_SALSA_STEP("f13", "f12", "f15", 9, 23)
        LTFARM_SALSA_STEP("f3", "f2", "f1", 13, 19)
        LTFARM_SALSA_STEP("f4", "f7", "f6", 13, 19)
        LTFARM_SALSA_STEP("f9", "f8", "f11", 13, 19)
        LTFARM_SALSA_STEP("f14", "f13", "f12", 13, 19)
        LTFARM_SALSA_STEP("f0", "f3", "f2", 18, 14)
        LTFARM_SALSA_STEP("f5", "f4", "f7", 18, 14)
        LTFARM_SALSA_STEP("f10", "f9", "f8", 18, 14)
        LTFARM_SALSA_STEP("f15", "f14", "f13", 18, 14)
        LTFARM_SALSA_STEP("f4", "f0", "f12", 7, 25)
        LTFARM_SALSA_STEP("f9", "f5", "f1", 7, 25)
        LTFARM_SALSA_STEP("f14", "f10", "f6", 7, 25)
        LTFARM_SALSA_STEP("f3", "f15", "f11", 7, 25)
        LTFARM_SALSA_STEP("f8", "f4", "f0", 9, 23)
        LTFARM_SALSA_STEP("f13", "f9", "f5", 9, 23)
        LTFARM_SALSA_STEP("f2", "f14", "f10", 9, 23)
        LTFARM_SALSA_STEP("f7", "f3", "f15", 9, 23)
        LTFARM_SALSA_STEP("f12", "f8", "f4", 13, 19)
        LTFARM_SALSA_STEP("f1", "f13", "f9", 13, 19)
        LTFARM_SALSA_STEP("f6", "f2", "f14", 13, 19)
        LTFARM_SALSA_STEP("f11", "f7", "f3", 13, 19)
        LTFARM_SALSA_STEP("f0", "f12", "f8", 18, 14)
        LTFARM_SALSA_STEP("f5", "f1", "f13", 18, 14)
        LTFARM_SALSA_STEP("f10", "f6", "f2", 18, 14)
        LTFARM_SALSA_STEP("f15", "f11", "f7", 18, 14)
        LTFARM_SALSA_STEP("f1", "f0", "f3", 7, 25)
        LTFARM_SALSA_STEP("f6", "f5", "f4", 7, 25)
        LTFARM_SALSA_STEP("f11", "f10", "f9", 7, 25)
        LTFARM_SALSA_STEP("f12", "f15", "f14", 7, 25)
        LTFARM_SALSA_STEP("f2", "f1", "f0", 9, 23)
        LTFARM_SALSA_STEP("f7", "f6", "f5", 9, 23)
        LTFARM_SALSA_STEP("f8", "f11", "f10", 9, 23)
        LTFARM_SALSA_STEP("f13", "f12", "f15", 9, 23)
        LTFARM_SALSA_STEP("f3", "f2", "f1", 13, 19)
        LTFARM_SALSA_STEP("f4", "f7", "f6", 13, 19)
        LTFARM_SALSA_STEP("f9", "f8", "f11", 13, 19)
        LTFARM_SALSA_STEP("f14", "f13", "f12", 13, 19)
        LTFARM_SALSA_STEP("f0", "f3", "f2", 18, 14)
        LTFARM_SALSA_STEP("f5", "f4", "f7", 18, 14)
        LTFARM_SALSA_STEP("f10", "f9", "f8", 18, 14)
        LTFARM_SALSA_STEP("f15", "f14", "f13", 18, 14)
        LTFARM_SALSA_STEP("f4", "f0", "f12", 7, 25)
        LTFARM_SALSA_STEP("f9", "f5", "f1", 7, 25)
        LTFARM_SALSA_STEP("f14", "f10", "f6", 7, 25)
        LTFARM_SALSA_STEP("f3", "f15", "f11", 7, 25)
        LTFARM_SALSA_STEP("f8", "f4", "f0", 9, 23)
        LTFARM_SALSA_STEP("f13", "f9", "f5", 9, 23)
        LTFARM_SALSA_STEP("f2", "f14", "f10", 9, 23)
        LTFARM_SALSA_STEP("f7", "f3", "f15", 9, 23)
        LTFARM_SALSA_STEP("f12", "f8", "f4", 13, 19)
        LTFARM_SALSA_STEP("f1", "f13", "f9", 13, 19)
        LTFARM_SALSA_STEP("f6", "f2", "f14", 13, 19)
        LTFARM_SALSA_STEP("f11", "f7", "f3", 13, 19)
        LTFARM_SALSA_STEP("f0", "f12", "f8", 18, 14)
        LTFARM_SALSA_STEP("f5", "f1", "f13", 18, 14)
        LTFARM_SALSA_STEP("f10", "f6", "f2", 18, 14)
        LTFARM_SALSA_STEP("f15", "f11", "f7", 18, 14)
        LTFARM_SALSA_STEP("f1", "f0", "f3", 7, 25)
        LTFARM_SALSA_STEP("f6", "f5", "f4", 7, 25)
        LTFARM_SALSA_STEP("f11", "f10", "f9", 7, 25)
        LTFARM_SALSA_STEP("f12", "f15", "f14", 7, 25)
        LTFARM_SALSA_STEP("f2", "f1", "f0", 9, 23)
        LTFARM_SALSA_STEP("f7", "f6", "f5", 9, 23)
        LTFARM_SALSA_STEP("f8", "f11", "f10", 9, 23)
        LTFARM_SALSA_STEP("f13", "f12", "f15", 9, 23)
        LTFARM_SALSA_STEP("f3", "f2", "f1", 13, 19)
        LTFARM_SALSA_STEP("f4", "f7", "f6", 13, 19)
        LTFARM_SALSA_STEP("f9", "f8", "f11", 13, 19)
        LTFARM_SALSA_STEP("f14", "f13", "f12", 13, 19)
        LTFARM_SALSA_STEP("f0", "f3", "f2", 18, 14)
        LTFARM_SALSA_STEP("f5", "f4", "f7", 18, 14)
        LTFARM_SALSA_STEP("f10", "f9", "f8", 18, 14)
        LTFARM_SALSA_STEP("f15", "f14", "f13", 18, 14)
        "fsw.ps f0, 0(%[x0])\n"
        "fsw.ps f1, 0(%[x1])\n"
        "fsw.ps f2, 0(%[x2])\n"
        "fsw.ps f3, 0(%[x3])\n"
        "fsw.ps f4, 0(%[x4])\n"
        "fsw.ps f5, 0(%[x5])\n"
        "fsw.ps f6, 0(%[x6])\n"
        "fsw.ps f7, 0(%[x7])\n"
        "fsw.ps f8, 0(%[x8])\n"
        "fsw.ps f9, 0(%[x9])\n"
        "fsw.ps f10, 0(%[x10])\n"
        "fsw.ps f11, 0(%[x11])\n"
        "fsw.ps f12, 0(%[x12])\n"
        "fsw.ps f13, 0(%[x13])\n"
        "fsw.ps f14, 0(%[x14])\n"
        "fsw.ps f15, 0(%[x15])\n"
        :
        : [x0] "r"(x[0]), [x1] "r"(x[1]), [x2] "r"(x[2]), [x3] "r"(x[3]),
          [x4] "r"(x[4]), [x5] "r"(x[5]), [x6] "r"(x[6]), [x7] "r"(x[7]),
          [x8] "r"(x[8]), [x9] "r"(x[9]), [x10] "r"(x[10]), [x11] "r"(x[11]),
          [x12] "r"(x[12]), [x13] "r"(x[13]), [x14] "r"(x[14]), [x15] "r"(x[15])
        : "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
          "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
          "f16", "f17", "memory");

    for (w = 0u; w < 16u; ++w)
        vec_add_u32x8(b[w], x[w]);
}

static void scrypt_1024_1_1_256_batch(const ltfarm_job_input_t *input,
                                      ltfarm_hash_batch_t *output,
                                      uint8_t scratchpad[LTFARM_SCRYPT_SCRATCHPAD_BYTES])
{
    uint8_t b[LTFARM_SCRYPT_LANES][128] __attribute__((aligned(32)));
    uint32_t inner_base[8][LTFARM_SCRYPT_LANES] __attribute__((aligned(32)));
    uint32_t outer_base[8][LTFARM_SCRYPT_LANES] __attribute__((aligned(32)));
    uint32_t x[32][LTFARM_SCRYPT_LANES] __attribute__((aligned(32)));
    uint32_t gather_offsets[LTFARM_SCRYPT_LANES] __attribute__((aligned(32)));
    uint32_t (*v)[32][LTFARM_SCRYPT_LANES];
    uint32_t lane;
    uint32_t i;
    uint32_t k;
    const uint32_t rom_entry_bytes =
        LTFARM_SCRYPT_ROM_ENTRY_WORDS *
        LTFARM_SCRYPT_LANES *
        (uint32_t)sizeof(uint32_t);

    v = (uint32_t (*)[32][LTFARM_SCRYPT_LANES])
        (((uintptr_t)scratchpad + 63u) & ~(uintptr_t)63u);

    hmac_sha256_prepare_bases_batch(input->header, inner_base, outer_base);
    hmac_sha256_84_x4_batch(inner_base, outer_base, input->header, b);

    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane)
        for (k = 0u; k < 32u; ++k)
            x[k][lane] = le32dec(&b[lane][k * 4u]);

    vec_enable_all_lanes();
    for (i = 0u; i < LTFARM_SCRYPT_ROM_ENTRIES; ++i) {
        for (k = 0u; k < 32u; ++k)
            vec_copy_u32x8(v[i][k], x[k]);
        xor_salsa8x8(&x[0], &x[16]);
        xor_salsa8x8(&x[16], &x[0]);
    }

    vec_enable_all_lanes();
    for (i = 0u; i < LTFARM_SCRYPT_ROM_ENTRIES; ++i) {
        for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane)
            gather_offsets[lane] =
                (x[16][lane] & (LTFARM_SCRYPT_ROM_ENTRIES - 1u)) *
                rom_entry_bytes +
                (lane * (uint32_t)sizeof(uint32_t));

        for (k = 0u; k < 32u; ++k) {
            vec_gather_xor_u32x8(
                x[k],
                (const uint8_t *)(const void *)&v[0][k][0],
                gather_offsets);
        }

        xor_salsa8x8(&x[0], &x[16]);
        xor_salsa8x8(&x[16], &x[0]);
    }

    output->count = input->count;
    output->reserved = 0u;
    for (lane = 0u; lane < LTFARM_SCRYPT_LANES; ++lane) {
        for (k = 0u; k < 32u; ++k)
            le32enc(&b[lane][k * 4u], x[k][lane]);
    }
    hmac_sha256_132_x1_batch(inner_base, outer_base, b, output->hash);
}

tpa_op_t ltfarm_scrypt_worker_start(void)
{
    ltfarm_scrypt_worker_ws_t *w = tpa_ws();

    if (!w)
        ltfarm_fail("LTWRKWS\n");

    return tpa_recv(tpa_chan(0), (void **)&w->in, &w->in_len,
                    ltfarm_scrypt_worker_run);
}

static tpa_op_t ltfarm_scrypt_worker_run(void)
{
    ltfarm_scrypt_worker_ws_t *w = tpa_ws();
    const ltfarm_job_input_t *in;

    if (!w)
        ltfarm_fail("LTWRKW\n");
    if (w->in_len != sizeof(ltfarm_job_input_t))
        ltfarm_fail("LTWRKL\n");
    in = (const ltfarm_job_input_t *)(const void *)w->in;
    if (!in)
        ltfarm_fail("LTWRKI\n");
    if (in->count == 0u || in->count > LTFARM_SCRYPT_LANES)
        ltfarm_fail("LTWRKC\n");

    scrypt_1024_1_1_256_batch(in, &w->out, worker_scratchpad);
    return tpa_send(tpa_chan(1), &w->out, sizeof(w->out),
                    ltfarm_scrypt_worker_done);
}

static tpa_op_t ltfarm_scrypt_worker_done(void)
{
    return tpa_stop();
}

/*-------------------------------------------------------------------------
 * Copyright (c) 2025 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-------------------------------------------------------------------------*/

#ifndef TPA_IMAGE_H
#define TPA_IMAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tpa_proc;
struct tpa_port_ref;

enum {
    TPA_PORT_IN  = 1u << 0,
    TPA_PORT_OUT = 1u << 1,
};

struct tpa_port {
    uint16_t id;
    uint16_t flags;
};

struct tpa_pdef {
    uint64_t id;
    tpa_cont_t start;
    uint32_t ws_sz;
    uint16_t nr_ports;
    uint16_t _pad;
    const struct tpa_port *ports;
};

struct tpa_inst {
    uint64_t id;
    const struct tpa_pdef *pdef;
};

struct tpa_end {
    uint64_t inst;
    uint16_t port;
    uint16_t _pad;
};

struct tpa_conn {
    struct tpa_end src;
    struct tpa_end dst;
    uint32_t bytes;
};

struct tpa_prog {
    uint32_t nr_insts;
    uint32_t nr_conns;
    const struct tpa_inst *instv;
    const struct tpa_conn *connv;
};

struct tpa_boot_ent {
    uint32_t hartid;
    uint16_t slot;
    uint16_t nr_portv;
    tpa_cont_t start;
    void *ws;
    const struct tpa_port_ref *portv;
    struct tpa_proc *proc;
};

enum {
    TPA_PROC_MEM_META_V1 = 1u,
};

#define TPA_PROC_MEM_META_MAGIC UINT64_C(0x315f4154454d5054)

struct tpa_proc_mem_meta {
    uint64_t magic;
    uint64_t version;
    uint64_t pid;
    uint64_t scratch_peak_bytes;
};

#define TPA_CAT2(a, b)  a##b
#define TPA_CAT(a, b)   TPA_CAT2(a, b)
#define TPA_NR(a)       (sizeof(a) / sizeof((a)[0]))

#define TPA_PORT(pid, flags0)                                                \
    {                                                                        \
        .id = (uint16_t)(pid),                                               \
        .flags = (uint16_t)(flags0),                                         \
    }

#define TPA_INST(iid, pdef0)                                                 \
    {                                                                        \
        .id = (uint64_t)(iid),                                               \
        .pdef = &(pdef0),                                                    \
    }

#define TPA_CONN(src_i, src_p, dst_i, dst_p, bytes0)                         \
    {                                                                        \
        .src = {                                                             \
            .inst = (uint64_t)(src_i),                                       \
            .port = (uint16_t)(src_p),                                       \
        },                                                                   \
        .dst = {                                                             \
            .inst = (uint64_t)(dst_i),                                       \
            .port = (uint16_t)(dst_p),                                       \
        },                                                                   \
        .bytes = (uint32_t)(bytes0),                                         \
    }

#define TPA_PDEF_INIT(pid, k0, ws_sz0, nr_ports0, ports0)                    \
    .id = (uint64_t)(pid),                                                   \
    .start = (k0),                                                           \
    .ws_sz = (uint32_t)(ws_sz0),                                             \
    .nr_ports = (uint16_t)(nr_ports0),                                       \
    .ports = (ports0),

#define TPA_USER_PDEF(name, pid, k0, ws_sz0)                                 \
    static const struct tpa_pdef name                                        \
    __attribute__((used, section(".tpa.proc.user"), aligned(8))) = {         \
        TPA_PDEF_INIT((pid), (k0), (ws_sz0), 0, 0)                           \
    }

#define TPA_USER_PDEF_PORTS(name, pid, k0, ws_sz0, ports0)                   \
    static const struct tpa_pdef name                                        \
    __attribute__((used, section(".tpa.proc.user"), aligned(8))) = {         \
        TPA_PDEF_INIT((pid), (k0), (ws_sz0), TPA_NR(ports0), (ports0))       \
    }

#define TPA_SYS_PDEF(name, pid, k0, ws_sz0)                                  \
    static const struct tpa_pdef name                                        \
    __attribute__((used, section(".tpa.proc.sys"), aligned(8))) = {          \
        TPA_PDEF_INIT((pid), (k0), (ws_sz0), 0, 0)                           \
    }

#define TPA_SYS_PDEF_PORTS(name, pid, k0, ws_sz0, ports0)                    \
    static const struct tpa_pdef name                                        \
    __attribute__((used, section(".tpa.proc.sys"), aligned(8))) = {          \
        TPA_PDEF_INIT((pid), (k0), (ws_sz0), TPA_NR(ports0), (ports0))       \
    }

#define TPA_USER_PROC(pid, k0, ws_sz0)                                       \
    TPA_USER_PDEF(TPA_CAT(__tpa_pdef_user_, __LINE__),                       \
                  (pid), (k0), (ws_sz0))

#define TPA_USER_PROC_PORTS(pid, k0, ws_sz0, ports0)                         \
    TPA_USER_PDEF_PORTS(TPA_CAT(__tpa_pdef_user_, __LINE__),                 \
                        (pid), (k0), (ws_sz0), (ports0))

#define TPA_SYS_PROC(pid, k0, ws_sz0)                                        \
    TPA_SYS_PDEF(TPA_CAT(__tpa_pdef_sys_, __LINE__),                         \
                 (pid), (k0), (ws_sz0))

#define TPA_SYS_PROC_PORTS(pid, k0, ws_sz0, ports0)                          \
    TPA_SYS_PDEF_PORTS(TPA_CAT(__tpa_pdef_sys_, __LINE__),                   \
                       (pid), (k0), (ws_sz0), (ports0))

#define TPA_BOOT(hartid0, slot0, k0, ws0, portv0, nr_portv0, pobj)           \
    static const struct tpa_boot_ent TPA_CAT(__tpa_boot_, __LINE__)          \
    __attribute__((used, section(".tpa.boot"), aligned(8))) = {              \
        .hartid = (uint32_t)(hartid0),                                       \
        .slot = (uint16_t)(slot0),                                           \
        .nr_portv = (uint16_t)(nr_portv0),                                   \
        .start = (k0),                                                       \
        .ws = (ws0),                                                         \
        .portv = (portv0),                                                   \
        .proc = &(pobj),                                                     \
    }

#define TPA_PROG(name, instv0, nr_insts0, connv0, nr_conns0)                 \
    static const struct tpa_prog name                                        \
    __attribute__((used, section(".tpa.prog"), aligned(8))) = {              \
        .nr_insts = (uint32_t)(nr_insts0),                                   \
        .nr_conns = (uint32_t)(nr_conns0),                                   \
        .instv = (instv0),                                                   \
        .connv = (connv0),                                                   \
    }

#define TPA_PROC_MEM_META(name, pid0, scratch0)                              \
    static const struct tpa_proc_mem_meta name                               \
    __attribute__((used, section(".tpa.proc.meta"), aligned(8))) = {         \
        .magic = TPA_PROC_MEM_META_MAGIC,                                    \
        .version = TPA_PROC_MEM_META_V1,                                     \
        .pid = (uint64_t)(pid0),                                             \
        .scratch_peak_bytes = (uint64_t)(scratch0),                          \
    }

#ifdef __cplusplus
}
#endif

#endif /* TPA_IMAGE_H */

if (NOT DEFINED PROCESS_MANIFESTS)
    message(FATAL_ERROR "gen_tpa_image.cmake needs PROCESS_MANIFESTS")
endif()

if (NOT DEFINED PROGRAM)
    message(FATAL_ERROR "gen_tpa_image.cmake needs PROGRAM")
endif()

if (NOT DEFINED PLACEMENT)
    message(FATAL_ERROR "gen_tpa_image.cmake needs PLACEMENT")
endif()

if (NOT DEFINED OUTPUT)
    message(FATAL_ERROR "gen_tpa_image.cmake needs OUTPUT")
endif()

set(use_edge_config FALSE)
if (DEFINED EDGE_CONFIG_HEADER AND NOT EDGE_CONFIG_HEADER STREQUAL "")
    set(use_edge_config TRUE)
endif()

function(tpa_split_lines path out_var)
    file(STRINGS "${path}" raw_lines)
    set(lines)

    foreach (line IN LISTS raw_lines)
        string(STRIP "${line}" line)

        if (line STREQUAL "")
            continue()
        endif()

        if (line MATCHES "^[ \t]*#")
            continue()
        endif()

        list(APPEND lines "${line}")
    endforeach()

    set(${out_var} "${lines}" PARENT_SCOPE)
endfunction()

function(tpa_find_pdef pdef_name idx_var)
    set(idx -1)
    list(LENGTH pdef_names npdefs)

    if (npdefs GREATER 0)
        math(EXPR last "${npdefs} - 1")
        foreach (i RANGE ${last})
            list(GET pdef_names ${i} cur)
            if (cur STREQUAL "${pdef_name}")
                set(idx ${i})
                break()
            endif()
        endforeach()
    endif()

    set(${idx_var} ${idx} PARENT_SCOPE)
endfunction()

function(tpa_find_inst inst_id idx_var)
    set(idx -1)
    list(LENGTH inst_ids ninsts)

    if (ninsts GREATER 0)
        math(EXPR last "${ninsts} - 1")
        foreach (i RANGE ${last})
            list(GET inst_ids ${i} cur)
            if (cur STREQUAL "${inst_id}")
                set(idx ${i})
                break()
            endif()
        endforeach()
    endif()

    set(${idx_var} ${idx} PARENT_SCOPE)
endfunction()

function(tpa_find_place inst_id idx_var)
    set(idx -1)
    list(LENGTH place_inst_ids nplaces)

    if (nplaces GREATER 0)
        math(EXPR last "${nplaces} - 1")
        foreach (i RANGE ${last})
            list(GET place_inst_ids ${i} cur)
            if (cur STREQUAL "${inst_id}")
                set(idx ${i})
                break()
            endif()
        endforeach()
    endif()

    set(${idx_var} ${idx} PARENT_SCOPE)
endfunction()

function(tpa_find_chan_override src_inst src_port dst_inst dst_port kind_var idx_var)
    set(kind "")
    set(idx -1)
    list(LENGTH chan_override_src_insts nchan_overrides)

    if (nchan_overrides GREATER 0)
        math(EXPR last "${nchan_overrides} - 1")
        foreach (i RANGE ${last})
            list(GET chan_override_src_insts ${i} cur_src_inst)
            list(GET chan_override_src_ports ${i} cur_src_port)
            list(GET chan_override_dst_insts ${i} cur_dst_inst)
            list(GET chan_override_dst_ports ${i} cur_dst_port)

            if (cur_src_inst STREQUAL "${src_inst}" AND
                cur_src_port STREQUAL "${src_port}" AND
                cur_dst_inst STREQUAL "${dst_inst}" AND
                cur_dst_port STREQUAL "${dst_port}")
                list(GET chan_override_kinds ${i} kind)
                set(idx ${i})
                break()
            endif()
        endforeach()
    endif()

    set(${kind_var} "${kind}" PARENT_SCOPE)
    set(${idx_var} ${idx} PARENT_SCOPE)
endfunction()

function(tpa_find_pdef_port pdef_idx port_id flags_var)
    set(ids_var "pdef_port_ids_${pdef_idx}")
    set(flgs_var "pdef_port_flags_${pdef_idx}")
    set(ids "${${ids_var}}")
    set(flgs "${${flgs_var}}")
    set(flags -1)
    list(LENGTH ids nports)

    if (nports GREATER 0)
        math(EXPR last "${nports} - 1")
        foreach (i RANGE ${last})
            list(GET ids ${i} cur_id)
            if (cur_id STREQUAL "${port_id}")
                list(GET flgs ${i} flags)
                break()
            endif()
        endforeach()
    endif()

    set(${flags_var} ${flags} PARENT_SCOPE)
endfunction()

tpa_split_lines("${PROGRAM}" program_lines)
tpa_split_lines("${PLACEMENT}" placement_lines)

string(REPLACE "|" ";" process_manifest_paths "${PROCESS_MANIFESTS}")

set(pdef_names)
set(pdef_kinds)
set(pdef_ids)
set(pdef_starts)
set(pdef_ws_szs)

set(inst_ids)
set(inst_pdefs)

set(conn_src_insts)
set(conn_src_ports)
set(conn_dst_insts)
set(conn_dst_ports)
set(conn_bytes)

foreach (manifest IN LISTS process_manifest_paths)
    tpa_split_lines("${manifest}" manifest_lines)

    foreach (line IN LISTS manifest_lines)
        string(REGEX REPLACE "[ \t]+" ";" fields "${line}")
        list(GET fields 0 tag)

        if (tag STREQUAL "pdef")
            list(LENGTH fields nfields)
            if (NOT nfields EQUAL 6)
                message(FATAL_ERROR
                    "bad pdef line: '${line}'\n"
                    "expected: pdef <name> <user|sys> <pid> <start> <ws_sz>"
                )
            endif()

            list(GET fields 1 name)
            list(GET fields 2 kind)
            list(GET fields 3 pid)
            list(GET fields 4 start)
            list(GET fields 5 ws_sz)

            if (NOT kind STREQUAL "user" AND NOT kind STREQUAL "sys")
                message(FATAL_ERROR "bad pdef kind '${kind}' in '${line}'")
            endif()

            if (NOT pid MATCHES "^[0-9]+$")
                message(FATAL_ERROR "bad pdef id '${pid}' in '${line}'")
            endif()

            if (NOT ws_sz MATCHES "^[0-9]+$")
                message(FATAL_ERROR "bad ws_sz '${ws_sz}' in '${line}'")
            endif()

            list(APPEND pdef_names "${name}")
            list(APPEND pdef_kinds "${kind}")
            list(APPEND pdef_ids "${pid}")
            list(APPEND pdef_starts "${start}")
            list(APPEND pdef_ws_szs "${ws_sz}")

            list(LENGTH pdef_names npdefs)
            math(EXPR pdef_idx "${npdefs} - 1")
            set(pdef_port_ids_${pdef_idx})
            set(pdef_port_flags_${pdef_idx})
        elseif (tag STREQUAL "port")
            list(LENGTH fields nfields)
            if (NOT nfields EQUAL 4)
                message(FATAL_ERROR
                    "bad port line: '${line}'\n"
                    "expected: port <pdef_name> <port_id> <in|out|inout>"
                )
            endif()

            list(GET fields 1 pdef_name)
            list(GET fields 2 port_id)
            list(GET fields 3 dir)

            if (NOT port_id MATCHES "^[0-9]+$")
                message(FATAL_ERROR "bad port id '${port_id}' in '${line}'")
            endif()

            tpa_find_pdef("${pdef_name}" pdef_idx)
            if (pdef_idx LESS 0)
                message(FATAL_ERROR
                    "port references unknown pdef '${pdef_name}'")
            endif()

            if (dir STREQUAL "in")
                set(flags 1)
            elseif (dir STREQUAL "out")
                set(flags 2)
            elseif (dir STREQUAL "inout")
                set(flags 3)
            else()
                message(FATAL_ERROR "bad port dir '${dir}' in '${line}'")
            endif()

            set(ids_var "pdef_port_ids_${pdef_idx}")
            set(flgs_var "pdef_port_flags_${pdef_idx}")
            set(ids "${${ids_var}}")
            set(flgs "${${flgs_var}}")
            list(FIND ids "${port_id}" dup)
            if (NOT dup EQUAL -1)
                message(FATAL_ERROR
                    "duplicate port ${port_id} on pdef '${pdef_name}'")
            endif()

            list(APPEND ids "${port_id}")
            list(APPEND flgs "${flags}")
            set(${ids_var} "${ids}")
            set(${flgs_var} "${flgs}")
        else()
            message(FATAL_ERROR
                "unknown process-manifest tag '${tag}' in '${line}'")
        endif()
    endforeach()
endforeach()

foreach (line IN LISTS program_lines)
    string(REGEX REPLACE "[ \t]+" ";" fields "${line}")
    list(GET fields 0 tag)

    if (tag STREQUAL "inst")
        list(LENGTH fields nfields)
        if (NOT nfields EQUAL 3)
            message(FATAL_ERROR
                "bad inst line: '${line}'\n"
                "expected: inst <inst_id> <pdef_name>"
            )
        endif()

        list(GET fields 1 inst_id)
        list(GET fields 2 pdef_name)

        if (NOT inst_id MATCHES "^[0-9]+$")
            message(FATAL_ERROR "bad inst id '${inst_id}' in '${line}'")
        endif()

        tpa_find_pdef("${pdef_name}" pdef_idx)
        if (pdef_idx LESS 0)
            message(FATAL_ERROR
                "inst '${inst_id}' references unknown pdef '${pdef_name}'")
        endif()

        tpa_find_inst("${inst_id}" inst_idx)
        if (NOT inst_idx LESS 0)
            message(FATAL_ERROR "duplicate inst '${inst_id}'")
        endif()

        list(APPEND inst_ids "${inst_id}")
        list(APPEND inst_pdefs "${pdef_name}")
    elseif (tag STREQUAL "conn")
        list(LENGTH fields nfields)
        if (NOT nfields EQUAL 6)
            message(FATAL_ERROR
                "bad conn line: '${line}'\n"
                "expected: conn <src_inst> <src_port> <dst_inst> <dst_port> <bytes>"
            )
        endif()

        list(GET fields 1 src_inst)
        list(GET fields 2 src_port)
        list(GET fields 3 dst_inst)
        list(GET fields 4 dst_port)
        list(GET fields 5 bytes)

        if (NOT src_inst MATCHES "^[0-9]+$" OR
            NOT src_port MATCHES "^[0-9]+$" OR
            NOT dst_inst MATCHES "^[0-9]+$" OR
            NOT dst_port MATCHES "^[0-9]+$" OR
            NOT bytes MATCHES "^[0-9]+$")
            message(FATAL_ERROR "bad conn line '${line}'")
        endif()

        if (bytes LESS 1)
            message(FATAL_ERROR "conn bytes must be > 0 in '${line}'")
        endif()

        tpa_find_inst("${src_inst}" inst_idx)
        if (inst_idx LESS 0)
            message(FATAL_ERROR "conn source inst '${src_inst}' not found")
        endif()

        tpa_find_inst("${dst_inst}" inst_idx)
        if (inst_idx LESS 0)
            message(FATAL_ERROR "conn dest inst '${dst_inst}' not found")
        endif()

        list(APPEND conn_src_insts "${src_inst}")
        list(APPEND conn_src_ports "${src_port}")
        list(APPEND conn_dst_insts "${dst_inst}")
        list(APPEND conn_dst_ports "${dst_port}")
        list(APPEND conn_bytes "${bytes}")
    else()
        message(FATAL_ERROR "unknown program-manifest tag '${tag}' in '${line}'")
    endif()
endforeach()

set(place_inst_ids)
set(place_pdef_names)
set(place_kinds)
set(place_starts)
set(place_ws_szs)
set(place_hartids)
set(place_slots)
set(place_ws_syms)
set(place_proc_syms)

set(chan_override_src_insts)
set(chan_override_src_ports)
set(chan_override_dst_insts)
set(chan_override_dst_ports)
set(chan_override_kinds)
set(chan_override_used)

set(ext_starts)
set(pdef_defs)
set(inst_defs)
set(conn_defs)
set(chan_defs)
set(chan_buf_defs)
set(portv_defs)
set(ws_defs)
set(proc_defs)
set(boot_defs)
set(boot_vector_defs)

list(LENGTH pdef_names npdefs)
if (npdefs GREATER 0)
    math(EXPR last_pdef "${npdefs} - 1")
    foreach (i RANGE ${last_pdef})
        list(GET pdef_names ${i} name)
        list(GET pdef_kinds ${i} kind)
        list(GET pdef_ids ${i} pid)
        list(GET pdef_starts ${i} start)
        list(GET pdef_ws_szs ${i} ws_sz)

        list(APPEND ext_starts "${start}")

        set(ids "${pdef_port_ids_${i}}")
        set(flgs "${pdef_port_flags_${i}}")
        set(portv_expr 0)
        set(nports 0)

        list(LENGTH ids nports)
        if (nports GREATER 0)
            set(portv_expr "__tpa_pdef_ports_${i}")
            string(APPEND pdef_defs
"static const struct tpa_port ${portv_expr}[] = {\n")

            math(EXPR last_port "${nports} - 1")
            foreach (j RANGE ${last_port})
                list(GET ids ${j} port_id)
                list(GET flgs ${j} flags)
                string(APPEND pdef_defs
"    { .id = ${port_id}, .flags = ${flags} },\n")
            endforeach()

            string(APPEND pdef_defs
"};\n\n")
        endif()

        if (kind STREQUAL "user")
            set(sec ".tpa.proc.user")
        else()
            set(sec ".tpa.proc.sys")
        endif()

        string(APPEND pdef_defs
"static const struct tpa_pdef ${name}\n"
"__attribute__((used, section(\"${sec}\"), aligned(8))) = {\n"
"    .id = ${pid}ull,\n"
"    .start = ${start},\n"
"    .ws_sz = ${ws_sz},\n"
"    .nr_ports = ${nports},\n"
"    .ports = ${portv_expr},\n"
"};\n\n")
    endforeach()
endif()

list(LENGTH inst_ids ninsts)
if (ninsts GREATER 0)
    math(EXPR last_inst "${ninsts} - 1")
    foreach (i RANGE ${last_inst})
        list(GET inst_ids ${i} inst_id)
        list(GET inst_pdefs ${i} pdef_name)

        string(APPEND inst_defs
"    TPA_INST(${inst_id}, ${pdef_name}),\n")
    endforeach()
endif()

list(LENGTH conn_src_insts nconns)
if (nconns GREATER 0)
    math(EXPR last_conn "${nconns} - 1")
    foreach (i RANGE ${last_conn})
        list(GET conn_src_insts ${i} src_inst)
        list(GET conn_src_ports ${i} src_port)
        list(GET conn_dst_insts ${i} dst_inst)
        list(GET conn_dst_ports ${i} dst_port)
        list(GET conn_bytes ${i} ch_bytes)
        list(GET conn_bytes ${i} bytes)

        string(APPEND conn_defs
"    TPA_CONN(${src_inst}, ${src_port}, ${dst_inst}, ${dst_port}, ${bytes}),\n")
    endforeach()
endif()

set(proc_idx 0)
foreach (line IN LISTS placement_lines)
    string(REGEX REPLACE "[ \t]+" ";" fields "${line}")
    list(LENGTH fields nfields)
    list(GET fields 0 tag)

    if (tag STREQUAL "chan")
        if (NOT nfields EQUAL 6)
            message(FATAL_ERROR
                "bad channel placement line: '${line}'\n"
                "expected: chan <src_inst> <src_port> <dst_inst> <dst_port> <direct|local|fabric|external>"
            )
        endif()

        list(GET fields 1 src_inst)
        list(GET fields 2 src_port)
        list(GET fields 3 dst_inst)
        list(GET fields 4 dst_port)
        list(GET fields 5 ch_kind)

        if (NOT src_inst MATCHES "^[0-9]+$" OR
            NOT src_port MATCHES "^[0-9]+$" OR
            NOT dst_inst MATCHES "^[0-9]+$" OR
            NOT dst_port MATCHES "^[0-9]+$")
            message(FATAL_ERROR "bad channel placement line '${line}'")
        endif()

        if (ch_kind STREQUAL "direct")
            set(ch_kind_expr TPA_CHANNEL_KIND_DIRECT)
        elseif (ch_kind STREQUAL "local")
            set(ch_kind_expr TPA_CHANNEL_KIND_LOCAL)
        elseif (ch_kind STREQUAL "fabric")
            set(ch_kind_expr TPA_CHANNEL_KIND_FABRIC)
        elseif (ch_kind STREQUAL "external")
            message(FATAL_ERROR
                "external channel kind is not implemented by this runtime: '${line}'")
        else()
            message(FATAL_ERROR "bad channel kind '${ch_kind}' in '${line}'")
        endif()

        tpa_find_chan_override("${src_inst}" "${src_port}" "${dst_inst}"
                               "${dst_port}" old_kind old_idx)
        if (NOT old_kind STREQUAL "")
            message(FATAL_ERROR "duplicate channel placement '${line}'")
        endif()

        list(APPEND chan_override_src_insts "${src_inst}")
        list(APPEND chan_override_src_ports "${src_port}")
        list(APPEND chan_override_dst_insts "${dst_inst}")
        list(APPEND chan_override_dst_ports "${dst_port}")
        list(APPEND chan_override_kinds "${ch_kind_expr}")
        list(APPEND chan_override_used 0)
        continue()
    endif()

    if (NOT nfields EQUAL 2)
        message(FATAL_ERROR
            "bad placement line: '${line}'\n"
            "expected: <inst_id> <hartid>"
        )
    endif()

    list(GET fields 0 inst_id)
    list(GET fields 1 hartid)

    if (NOT inst_id MATCHES "^[0-9]+$")
        message(FATAL_ERROR "bad inst id '${inst_id}' in '${line}'")
    endif()

    if (NOT hartid MATCHES "^[0-9]+$")
        message(FATAL_ERROR "bad hart id '${hartid}' in '${line}'")
    endif()

    tpa_find_place("${inst_id}" place_idx)
    if (NOT place_idx LESS 0)
        message(FATAL_ERROR "duplicate placement for inst '${inst_id}'")
    endif()

    if (hartid GREATER 65535)
        message(FATAL_ERROR "hart id ${hartid} out of range in '${line}'")
    endif()

    set(slot_var "slot_count_${hartid}")
    if (DEFINED ${slot_var})
        set(slot "${${slot_var}}")
    else()
        set(slot 0)
    endif()
    math(EXPR next_slot "${slot} + 1")
    set(${slot_var} "${next_slot}")

    tpa_find_inst("${inst_id}" inst_idx)
    if (inst_idx LESS 0)
        message(FATAL_ERROR
            "placement references unknown inst '${inst_id}'")
    endif()

    list(GET inst_pdefs ${inst_idx} pdef_name)
    tpa_find_pdef("${pdef_name}" pdef_idx)
    list(GET pdef_kinds ${pdef_idx} kind)
    list(GET pdef_starts ${pdef_idx} start)
    list(GET pdef_ws_szs ${pdef_idx} ws_sz)

    list(APPEND place_inst_ids "${inst_id}")
    list(APPEND place_pdef_names "${pdef_name}")
    list(APPEND place_kinds "${kind}")
    list(APPEND place_starts "${start}")
    list(APPEND place_ws_szs "${ws_sz}")
    list(APPEND place_hartids "${hartid}")
    list(APPEND place_slots "${slot}")
    list(APPEND place_ws_syms "__tpa_ws_${proc_idx}")
    list(APPEND place_proc_syms "__tpa_boot_proc_${proc_idx}")

    math(EXPR proc_idx "${proc_idx} + 1")
endforeach()

if (ninsts GREATER 0)
    math(EXPR last_inst "${ninsts} - 1")
    foreach (i RANGE ${last_inst})
        list(GET inst_ids ${i} inst_id)
        tpa_find_place("${inst_id}" place_idx)
        if (place_idx LESS 0)
            message(FATAL_ERROR "inst '${inst_id}' has no placement")
        endif()
    endforeach()
endif()

if (nconns GREATER 0)
    math(EXPR last_conn "${nconns} - 1")
    foreach (i RANGE ${last_conn})
        list(GET conn_src_insts ${i} src_inst)
        list(GET conn_src_ports ${i} src_port)
        list(GET conn_dst_insts ${i} dst_inst)
        list(GET conn_dst_ports ${i} dst_port)
        list(GET conn_bytes ${i} ch_bytes)

        tpa_find_place("${src_inst}" src_place)
        tpa_find_place("${dst_inst}" dst_place)
        if (src_place LESS 0 OR dst_place LESS 0)
            message(FATAL_ERROR "conn references unplaced inst")
        endif()

        list(GET place_pdef_names ${src_place} src_pdef_name)
        list(GET place_pdef_names ${dst_place} dst_pdef_name)
        tpa_find_pdef("${src_pdef_name}" src_pdef_idx)
        tpa_find_pdef("${dst_pdef_name}" dst_pdef_idx)

        tpa_find_pdef_port(${src_pdef_idx} "${src_port}" src_flags)
        tpa_find_pdef_port(${dst_pdef_idx} "${dst_port}" dst_flags)

        if (src_flags LESS 0)
            message(FATAL_ERROR
                "inst '${src_inst}' uses undefined src port ${src_port}")
        endif()

        if (dst_flags LESS 0)
            message(FATAL_ERROR
                "inst '${dst_inst}' uses undefined dst port ${dst_port}")
        endif()

        math(EXPR src_ok "${src_flags} & 2")
        math(EXPR dst_ok "${dst_flags} & 1")
        if (NOT src_ok)
            message(FATAL_ERROR
                "src port ${src_port} on inst '${src_inst}' is not output")
        endif()

        if (NOT dst_ok)
            message(FATAL_ERROR
                "dst port ${dst_port} on inst '${dst_inst}' is not input")
        endif()

        list(GET place_hartids ${src_place} src_hartid)
        list(GET place_slots ${src_place} src_slot)
        list(GET place_hartids ${dst_place} dst_hartid)
        list(GET place_slots ${dst_place} dst_slot)

        tpa_find_chan_override("${src_inst}" "${src_port}" "${dst_inst}"
                               "${dst_port}" ch_kind ch_override_idx)
        if (ch_kind STREQUAL "")
            set(ch_kind "TPA_CHANNEL_KIND(${src_hartid}, ${dst_hartid})")
        else()
            list(REMOVE_AT chan_override_used ${ch_override_idx})
            list(INSERT chan_override_used ${ch_override_idx} 1)
        endif()

        set(ch_sym "__tpa_ch_${i}")
        if (use_edge_config)
            string(APPEND chan_defs
"#if TPA_EDGE_CH_${i}_NRBUF > 2u\n"
"#error \"TPA_EDGE_CH_${i}_NRBUF exceeds tpa_chan buffer vector capacity\"\n"
"#endif\n"
"#if (${ch_kind}) == TPA_CHANNEL_KIND_FABRIC && TPA_EDGE_CH_${i}_NRBUF == 0u\n"
"#error \"fabric channel ${i} needs at least one edge buffer\"\n"
"#endif\n"
"#if (${ch_kind}) == TPA_CHANNEL_KIND_EXTERNAL\n"
"#error \"external channel ${i} is not implemented by this runtime\"\n"
"#endif\n"
"static struct tpa_channel ${ch_sym} __attribute__((aligned(64))) = {\n"
"    .kind = ${ch_kind},\n"
"    .nrbuf = TPA_EDGE_CH_${i}_NRBUF,\n"
"    .cap = ${ch_bytes},\n"
"    .tx = { .hartid = ${src_hartid}, .slot = ${src_slot} },\n"
"    .rx = { .hartid = ${dst_hartid}, .slot = ${dst_slot} },\n"
"    .bufv = { TPA_EDGE_CH_${i}_BUF0, TPA_EDGE_CH_${i}_BUF1 },\n"
"};\n\n")
        elseif (ch_kind STREQUAL "TPA_CHANNEL_KIND_FABRIC")
            set(ch_buf0 "__tpa_ch_${i}_buf0")
            set(ch_buf1 "__tpa_ch_${i}_buf1")
            string(APPEND chan_buf_defs
"static unsigned char ${ch_buf0}[${ch_bytes}] __attribute__((aligned(64)));\n"
"static unsigned char ${ch_buf1}[${ch_bytes}] __attribute__((aligned(64)));\n\n")
            string(APPEND chan_defs
"static struct tpa_channel ${ch_sym} __attribute__((aligned(64))) = {\n"
"    .kind = ${ch_kind},\n"
"    .nrbuf = 2,\n"
"    .cap = ${ch_bytes},\n"
"    .tx = { .hartid = ${src_hartid}, .slot = ${src_slot} },\n"
"    .rx = { .hartid = ${dst_hartid}, .slot = ${dst_slot} },\n"
"    .bufv = { ${ch_buf0}, ${ch_buf1} },\n"
"};\n\n")
        elseif (ch_kind MATCHES "^TPA_CHANNEL_KIND")
            set(ch_buf0 "__tpa_ch_${i}_buf0")
            set(ch_buf1 "__tpa_ch_${i}_buf1")
            string(APPEND chan_buf_defs
"#if (${ch_kind}) == TPA_CHANNEL_KIND_FABRIC\n"
"static unsigned char ${ch_buf0}[${ch_bytes}] __attribute__((aligned(64)));\n"
"static unsigned char ${ch_buf1}[${ch_bytes}] __attribute__((aligned(64)));\n"
"#endif\n\n")
            string(APPEND chan_defs
"#if (${ch_kind}) == TPA_CHANNEL_KIND_EXTERNAL\n"
"#error \"external channel ${i} is not implemented by this runtime\"\n"
"#endif\n"
"static struct tpa_channel ${ch_sym} __attribute__((aligned(64))) = {\n"
"    .kind = ${ch_kind},\n"
"#if (${ch_kind}) == TPA_CHANNEL_KIND_FABRIC\n"
"    .nrbuf = 2,\n"
"#endif\n"
"    .cap = ${ch_bytes},\n"
"    .tx = { .hartid = ${src_hartid}, .slot = ${src_slot} },\n"
"    .rx = { .hartid = ${dst_hartid}, .slot = ${dst_slot} },\n"
"#if (${ch_kind}) == TPA_CHANNEL_KIND_FABRIC\n"
"    .bufv = { ${ch_buf0}, ${ch_buf1} },\n"
"#endif\n"
"};\n\n")
        else()
            string(APPEND chan_defs
"static struct tpa_channel ${ch_sym} __attribute__((aligned(64))) = {\n"
"    .kind = ${ch_kind},\n"
"    .cap = ${ch_bytes},\n"
"    .tx = { .hartid = ${src_hartid}, .slot = ${src_slot} },\n"
"    .rx = { .hartid = ${dst_hartid}, .slot = ${dst_slot} },\n"
"};\n\n")
        endif()

        foreach (side IN ITEMS src dst)
            if (side STREQUAL "src")
                set(place_idx ${src_place})
                set(port_id "${src_port}")
            else()
                set(place_idx ${dst_place})
                set(port_id "${dst_port}")
            endif()

            set(ids_var "place_port_ids_${place_idx}")
            set(chs_var "place_port_chs_${place_idx}")
            set(ids "${${ids_var}}")
            set(chs "${${chs_var}}")
            list(FIND ids "${port_id}" dup)
            if (NOT dup EQUAL -1)
                list(GET place_inst_ids ${place_idx} inst_id0)
                message(FATAL_ERROR
                    "inst '${inst_id0}' port ${port_id} already bound")
            endif()

            list(APPEND ids "${port_id}")
            list(APPEND chs "${ch_sym}")
            set(${ids_var} "${ids}")
            set(${chs_var} "${chs}")
        endforeach()
    endforeach()
endif()

list(LENGTH chan_override_src_insts nchan_overrides)
if (nchan_overrides GREATER 0)
    math(EXPR last_chan_override "${nchan_overrides} - 1")
    foreach (i RANGE ${last_chan_override})
        list(GET chan_override_used ${i} used)
        if (NOT used)
            list(GET chan_override_src_insts ${i} src_inst)
            list(GET chan_override_src_ports ${i} src_port)
            list(GET chan_override_dst_insts ${i} dst_inst)
            list(GET chan_override_dst_ports ${i} dst_port)
            message(FATAL_ERROR
                "channel placement override ${src_inst}:${src_port} -> "
                "${dst_inst}:${dst_port} does not match any conn")
        endif()
    endforeach()
endif()

list(LENGTH place_inst_ids nplaces)
if (nplaces GREATER 0)
    math(EXPR last_place "${nplaces} - 1")
    foreach (i RANGE ${last_place})
        list(GET place_ws_szs ${i} ws_sz)
        list(GET place_ws_syms ${i} ws_sym)
        list(GET place_proc_syms ${i} proc_sym)
        list(GET place_starts ${i} start)
        list(GET place_hartids ${i} hartid)
        list(GET place_slots ${i} slot)

        set(ids "${place_port_ids_${i}}")
        set(chs "${place_port_chs_${i}}")
        set(portv_expr 0)
        set(nportv 0)

        if (ws_sz GREATER 0)
            string(APPEND ws_defs
"static unsigned char ${ws_sym}[${ws_sz}] __attribute__((aligned(64)));\n")
            set(ws_expr "${ws_sym}")
        else()
            set(ws_expr "0")
        endif()

        list(LENGTH ids nportv)
        if (nportv GREATER 0)
            set(portv_expr "__tpa_portv_${i}")
            string(APPEND portv_defs
"static const struct tpa_port_ref ${portv_expr}[] = {\n")
            math(EXPR last_bind "${nportv} - 1")
            foreach (j RANGE ${last_bind})
                list(GET ids ${j} port_id)
                list(GET chs ${j} ch_sym)
                string(APPEND portv_defs
"    { .id = ${port_id}, .ch = &${ch_sym} },\n")
            endforeach()
            string(APPEND portv_defs
"};\n\n")
        endif()

        string(APPEND proc_defs
"TPA_STATIC_ASSERT(${hartid} < TPA_HAL_NR_HARTS, \"placement hart ${hartid} exceeds TPA_HAL_NR_HARTS\");\n"
"TPA_STATIC_ASSERT(${slot} < TPA_PROCESS_MAX_PROCS_PER_HART, \"placement slot ${slot} exceeds TPA_PROCESS_MAX_PROCS_PER_HART\");\n"
"static struct tpa_proc ${proc_sym} = {\n"
"    .k = ${start},\n"
"    .ws = ${ws_expr},\n"
"    .portv = ${portv_expr},\n"
"    .hartid = ${hartid},\n"
"    .state = TPA_PROCESS_DEAD,\n"
"    .slot = ${slot},\n"
"    .nr_portv = ${nportv},\n"
"};\n\n")

        set(boot_sym "__tpa_boot_ent_${i}")
        string(APPEND boot_defs
"TPA_BOOT_NAMED(${boot_sym}, ${hartid}, ${slot}, ${start}, ${ws_expr}, ${portv_expr}, ${nportv}, ${proc_sym});\n\n")
        string(APPEND boot_vector_defs
"    &${boot_sym},\n")
    endforeach()
endif()

list(REMOVE_DUPLICATES ext_starts)

get_filename_component(out_dir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")

file(WRITE "${OUTPUT}"
"/* Auto-generated from ${PROGRAM} and ${PLACEMENT}. */\n"
"#include \"tpa/tpa.h\"\n")

if (use_edge_config)
    file(APPEND "${OUTPUT}" "#include \"${EDGE_CONFIG_HEADER}\"\n")
endif()

file(APPEND "${OUTPUT}" "\n")

foreach (start IN LISTS ext_starts)
    file(APPEND "${OUTPUT}" "extern tpa_op_t ${start}(void);\n")
endforeach()

if (ext_starts)
    file(APPEND "${OUTPUT}" "\n")
endif()

file(APPEND "${OUTPUT}" "${pdef_defs}")

if (ninsts GREATER 0)
    file(APPEND "${OUTPUT}"
"static const struct tpa_inst __tpa_prog_instv[] = {\n"
"${inst_defs}"
"};\n\n")
    set(instv_expr "__tpa_prog_instv")
else()
    set(instv_expr "0")
endif()

if (nconns GREATER 0)
    file(APPEND "${OUTPUT}"
"static const struct tpa_conn __tpa_prog_connv[] = {\n"
"${conn_defs}"
"};\n\n")
    set(connv_expr "__tpa_prog_connv")
else()
    set(connv_expr "0")
endif()

file(APPEND "${OUTPUT}"
"TPA_PROG(__tpa_prog_0, ${instv_expr}, ${ninsts}, ${connv_expr}, ${nconns});\n\n")

file(APPEND "${OUTPUT}" "${chan_buf_defs}")
file(APPEND "${OUTPUT}" "${chan_defs}")
file(APPEND "${OUTPUT}" "${portv_defs}")
file(APPEND "${OUTPUT}" "${ws_defs}")
if (chan_defs OR portv_defs OR ws_defs)
    file(APPEND "${OUTPUT}" "\n")
endif()

file(APPEND "${OUTPUT}" "${proc_defs}")
if (proc_defs)
    file(APPEND "${OUTPUT}" "\n")
endif()

file(APPEND "${OUTPUT}" "${boot_defs}")

if (nplaces GREATER 0)
    file(APPEND "${OUTPUT}"
"const struct tpa_boot_ent * const __tpa_boot_vector[]\n"
"__attribute__((used, retain, section(\".tpa.bootvec\"), aligned(8))) = {\n"
"${boot_vector_defs}"
"};\n"
"const uint32_t __tpa_boot_count\n"
"__attribute__((used, retain, section(\".tpa.bootvec\"), aligned(4))) = ${nplaces}u;\n")
else()
    file(APPEND "${OUTPUT}"
"const struct tpa_boot_ent * const __tpa_boot_vector[1]\n"
"__attribute__((used, retain, section(\".tpa.bootvec\"), aligned(8))) = { 0 };\n"
"const uint32_t __tpa_boot_count\n"
"__attribute__((used, retain, section(\".tpa.bootvec\"), aligned(4))) = 0u;\n")
endif()

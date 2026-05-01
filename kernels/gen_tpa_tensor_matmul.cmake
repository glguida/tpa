if (NOT DEFINED OUTPUT_PROGRAM)
    message(FATAL_ERROR "need OUTPUT_PROGRAM")
endif()

if (NOT DEFINED OUTPUT_PLACEMENT)
    message(FATAL_ERROR "need OUTPUT_PLACEMENT")
endif()

if (NOT DEFINED ROWS)
    set(ROWS 8)
endif()

if (NOT DEFINED COLS)
    set(COLS 8)
endif()

if (NOT DEFINED NR_MINIONS)
    set(NR_MINIONS 8)
endif()

if (NOT DEFINED TILE_BYTES)
    set(TILE_BYTES 16384)
endif()

function(a_feed_id col out_var)
    math(EXPR id "6000 + ${col}")
    set(${out_var} ${id} PARENT_SCOPE)
endfunction()

function(b_feed_id row out_var)
    math(EXPR id "7000 + ${row}")
    set(${out_var} ${id} PARENT_SCOPE)
endfunction()

function(cell_id row col out_var)
    math(EXPR id "8000 + (${row} * ${COLS}) + ${col}")
    set(${out_var} ${id} PARENT_SCOPE)
endfunction()

function(check_id row col out_var)
    math(EXPR id "10000 + (${row} * ${COLS}) + ${col}")
    set(${out_var} ${id} PARENT_SCOPE)
endfunction()

function(place_inst inst seq)
    math(EXPR minion "${seq} % ${NR_MINIONS}")
    math(EXPR hartid "${minion} * 2")
    file(APPEND "${OUTPUT_PLACEMENT}" "${inst} ${hartid}\n")
endfunction()

file(WRITE "${OUTPUT_PROGRAM}" "")
file(WRITE "${OUTPUT_PLACEMENT}" "")

math(EXPR last_row "${ROWS} - 1")
math(EXPR last_col "${COLS} - 1")

foreach (col RANGE ${last_col})
    a_feed_id(${col} inst)
    file(APPEND "${OUTPUT_PROGRAM}"
        "inst ${inst} tensor_matmul_a_feed_pdef\n")
    place_inst(${inst} ${col})
endforeach()

foreach (row RANGE ${last_row})
    b_feed_id(${row} inst)
    file(APPEND "${OUTPUT_PROGRAM}"
        "inst ${inst} tensor_matmul_b_feed_pdef\n")
    place_inst(${inst} ${row})
endforeach()

foreach (row RANGE ${last_row})
    foreach (col RANGE ${last_col})
        cell_id(${row} ${col} cell)
        check_id(${row} ${col} chk)
        math(EXPR seq "(${row} * ${COLS}) + ${col}")

        file(APPEND "${OUTPUT_PROGRAM}"
            "inst ${cell} tensor_matmul_cell_pdef\n"
            "inst ${chk} tensor_matmul_check_pdef\n")
        place_inst(${cell} ${seq})
        place_inst(${chk} ${seq})
    endforeach()
endforeach()

foreach (col RANGE ${last_col})
    a_feed_id(${col} src)
    cell_id(0 ${col} dst)
    file(APPEND "${OUTPUT_PROGRAM}"
        "conn ${src} 0 ${dst} 0 ${TILE_BYTES}\n")
endforeach()

foreach (row RANGE ${last_row})
    b_feed_id(${row} src)
    cell_id(${row} ${last_col} dst)
    file(APPEND "${OUTPUT_PROGRAM}"
        "conn ${src} 0 ${dst} 1 ${TILE_BYTES}\n")
endforeach()

foreach (row RANGE ${last_row})
    foreach (col RANGE ${last_col})
        cell_id(${row} ${col} cell)
        check_id(${row} ${col} chk)
        file(APPEND "${OUTPUT_PROGRAM}"
            "conn ${cell} 4 ${chk} 0 ${TILE_BYTES}\n")

        if (row LESS last_row)
            math(EXPR next_row "${row} + 1")
            cell_id(${next_row} ${col} south)
            file(APPEND "${OUTPUT_PROGRAM}"
                "conn ${cell} 2 ${south} 0 ${TILE_BYTES}\n")
        endif()

        if (col GREATER 0)
            math(EXPR prev_col "${col} - 1")
            cell_id(${row} ${prev_col} west)
            file(APPEND "${OUTPUT_PROGRAM}"
                "conn ${cell} 3 ${west} 1 ${TILE_BYTES}\n")
        endif()
    endforeach()
endforeach()

if (NOT DEFINED OUTPUT_PROGRAM)
    message(FATAL_ERROR "need OUTPUT_PROGRAM")
endif()

if (NOT DEFINED OUTPUT_PLACEMENT)
    message(FATAL_ERROR "need OUTPUT_PLACEMENT")
endif()

if (NOT DEFINED ROWS)
    set(ROWS 16)
endif()

if (NOT DEFINED COLS)
    set(COLS 16)
endif()

if (NOT DEFINED NR_MINIONS)
    set(NR_MINIONS 8)
endif()

if (NOT DEFINED TILE_BYTES)
    set(TILE_BYTES 9216)
endif()

function(cell_id row col out_var)
    math(EXPR id "3000 + (${row} * ${COLS}) + ${col}")
    set(${out_var} ${id} PARENT_SCOPE)
endfunction()

function(check_id row col out_var)
    math(EXPR id "5000 + (${row} * ${COLS}) + ${col}")
    set(${out_var} ${id} PARENT_SCOPE)
endfunction()

file(WRITE "${OUTPUT_PROGRAM}" "")
file(WRITE "${OUTPUT_PLACEMENT}" "")

math(EXPR last_row "${ROWS} - 1")
math(EXPR last_col "${COLS} - 1")

foreach (col RANGE ${last_col})
    math(EXPR inst "1000 + ${col}")
    math(EXPR minion "${col} % ${NR_MINIONS}")
    math(EXPR hartid "${minion} * 2")
    file(APPEND "${OUTPUT_PROGRAM}"
        "inst ${inst} systolic_north_feed_pdef\n")
    file(APPEND "${OUTPUT_PLACEMENT}"
        "${inst} ${hartid}\n")
endforeach()

foreach (row RANGE ${last_row})
    math(EXPR inst "2000 + ${row}")
    math(EXPR minion "${row} % ${NR_MINIONS}")
    math(EXPR hartid "${minion} * 2")
    file(APPEND "${OUTPUT_PROGRAM}"
        "inst ${inst} systolic_east_feed_pdef\n")
    file(APPEND "${OUTPUT_PLACEMENT}"
        "${inst} ${hartid}\n")
endforeach()

foreach (row RANGE ${last_row})
    foreach (col RANGE ${last_col})
        cell_id(${row} ${col} cell)
        check_id(${row} ${col} chk)
        math(EXPR minion "((${row} * ${COLS}) + ${col}) % ${NR_MINIONS}")
        math(EXPR hartid "${minion} * 2")

        file(APPEND "${OUTPUT_PROGRAM}"
            "inst ${cell} systolic_cell_pdef\n"
            "inst ${chk} systolic_check_pdef\n")
        file(APPEND "${OUTPUT_PLACEMENT}"
            "${cell} ${hartid}\n"
            "${chk} ${hartid}\n")
    endforeach()
endforeach()

foreach (col RANGE ${last_col})
    cell_id(0 ${col} cell)
    math(EXPR src "1000 + ${col}")
    file(APPEND "${OUTPUT_PROGRAM}"
        "conn ${src} 0 ${cell} 0 ${TILE_BYTES}\n")
endforeach()

foreach (row RANGE ${last_row})
    cell_id(${row} ${last_col} cell)
    math(EXPR src "2000 + ${row}")
    file(APPEND "${OUTPUT_PROGRAM}"
        "conn ${src} 0 ${cell} 1 ${TILE_BYTES}\n")
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

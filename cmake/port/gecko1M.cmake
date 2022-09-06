set(board gecko1M)

set(SUB_ARCH efr32fg12p)
set(SUB_ARCH_DIR EFR32FG12P)
set(SUB_ARCH_FLAGS "EFR32FG12P;EFR32FG12P433F1024GL125;EFR32_SERIES1_CONFIG2_MICRO" )
set(RAIL_LIB "rail_efr32xg12_gcc_release")
set(linker_script ${RODOS_DIR}/src/bare-metal/${SUB_ARCH}/scripts/${SUB_ARCH}.ld)

include(${CMAKE_CURRENT_LIST_DIR}/efr32fg1p.cmake)

project (os)
list(APPEND ASMS crtso crti liburiscv)
list(APPEND TRGTS ${PROJECT_NAME})

add_library(${PROJECT_NAME}
    src/asl.c
    src/pcb.c
    src/init.c
    src/scheduler.c
    src/syscall.c
    src/semaphores.c
    src/puod.c
    src/util.c
)
add_library(sub::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

if(CROSS_COMPILE)
    set(CMAKE_C_COMPILER ${RISCV_CC})
    # Force the MIPS linker to avoid the GCC fronted that screws things up
    set(CMAKE_C_LINK_EXECUTABLE "${RISCV_LD} <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
    set(CMAKE_ASM_COMPILER ${RISCV_CC})

    foreach(ASM ${ASMS})
        add_library(${ASM} ${RISCV_DIR_PREFIX}/share/uriscv/${ASM}.S)
        add_library(sub::${ASM} ALIAS ${ASM})
        list(APPEND TRGTS ${ASM})
    endforeach()
endif()

foreach(TRGT ${TRGTS})
    target_compile_options(${TRGT} PRIVATE ${CFLAGS_ALL})
    target_include_directories(${TRGT}
        PUBLIC ${PROJECT_SOURCE_DIR}/include
        PUBLIC ${RISCV_DIR_PREFIX}/include
    )
endforeach()

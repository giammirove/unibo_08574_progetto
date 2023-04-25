project(phase1)
add_executable(${PROJECT_NAME}
    src/p1test.c
)

set(CMAKE_C_COMPILER ${RISCV_CC})
# Force the MIPS linker to avoid the GCC fronted that screws things up
set(CMAKE_C_LINK_EXECUTABLE "${RISCV_LD} <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
target_compile_options(${PROJECT_NAME} PRIVATE ${CFLAGS_ALL} -nostartfiles -static)
target_link_options(${PROJECT_NAME} PRIVATE ${LDFLAGS_ALL})
target_include_directories(${PROJECT_NAME} PUBLIC ${RISCV_DIR_PREFIX}/include)
target_link_libraries(${PROJECT_NAME}
    sub::os
    sub::liburiscv
    sub::crtso
)

# convert the elf output binary into an executable uriscv3 kernel
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND uriscv-elf2uriscv -k ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Converting ELF ${PROJECT_NAME} into uMPS3 Kernel..."
)

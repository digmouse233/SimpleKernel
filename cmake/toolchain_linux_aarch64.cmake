
# This file is a part of Simple-XX/SimpleKernel (https://github.com/Simple-XX/SimpleKernel).
# 
# toolchain_linux_aarch64.cmake for Simple-XX/SimpleKernel.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR AARCH64)

# TODO
# GCC
find_program(GCC aarch64-none-eabi-gcc)
if (NOT GCC)
    message(FATAL_ERROR "aarch64-none-eabi-gcc not found.\n"
            "Run `brew install aarch64-none-eabi-gcc` to install the toolchain")
else ()
    message(STATUS "Found aarch64-none-eabi-gcc ${GCC}")
endif ()

set(TOOLCHAIN_PREFIX   aarch64-none-eabi-)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_READELF      ${TOOLCHAIN_PREFIX}readelf)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_LINKER       ${TOOLCHAIN_PREFIX}ld)
set(CMAKE_NM           ${TOOLCHAIN_PREFIX}nm)
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}ranlib)

# qemu
find_program(QEMU qemu-system-aarch64)
if (NOT QEMU)
    message(FATAL_ERROR "qemu not found.\n"
            "Please install qemu first.")
else ()
    message(STATUS "Found qemu ${QEMU}")
endif ()

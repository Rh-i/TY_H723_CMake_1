# CMSIS DSP Library for STM32H723 (Cortex-M7)
# This file creates an OBJECT library with all CMSIS-DSP source files.

set(CMSIS_DSP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/DSP)

# Collect all arm_*.c source files (excluding aggregate files like BasicMathFunctions.c)
# GLOB_RECURSE matches arm_*.c at any directory depth under Source/
file(GLOB_RECURSE CMSIS_DSP_SOURCES
    ${CMSIS_DSP_DIR}/Source/arm_*.c
)

# Create OBJECT library (compiles sources into .o but doesn't archive into .a)
add_library(CMSIS_DSP OBJECT ${CMSIS_DSP_SOURCES})

# Include paths: public headers, private headers
target_include_directories(CMSIS_DSP PUBLIC
    ${CMSIS_DSP_DIR}/Include
    ${CMSIS_DSP_DIR}/PrivateInclude
)

# CMSIS core headers already provided by stm32cubemx INTERFACE library
target_link_libraries(CMSIS_DSP PUBLIC stm32cubemx)

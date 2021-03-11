#******************************************************************************
#
# Makefile - Rules for building the FreeRTOS example.
#
# Copyright (c) 2012 Texas Instruments Incorporated.  All rights reserved.
# Software License Agreement
# 
# Texas Instruments (TI) is supplying this software for use solely and
# exclusively on TI's microcontroller products. The software is owned by
# TI and/or its suppliers, and is protected under applicable copyright
# laws. You may not combine this software with "viral" open-source
# software in order to form a larger program.
# 
# THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
# NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
# NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
# CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
# DAMAGES, FOR ANY REASON WHATSOEVER.
# 
# This is part of revision 9453 of the EK-LM4F120XL Firmware Package.
#
#******************************************************************************

#
# Defines the part type that this project uses.
#
PART=LM4F120H5QR

#
# Set the processor variant.
#
VARIANT=cm4f

#
# The base directory for StellarisWare.
#
ROOT=../../..

#
# Include the common make definitions.
#
include ${ROOT}/makedefs

#
# Where to find source files that do not live in this directory.
#
VPATH=../../../third_party/FreeRTOS/Source/portable/GCC/ARM_CM4F
VPATH+=../../../third_party/FreeRTOS/Source/portable/MemMang/
VPATH+=../../../third_party/FreeRTOS/Source
VPATH+=../drivers
VPATH+=../../../utils

#
# Where to find header files that do not live in the source directory.
#
IPATH=.
IPATH+=..
IPATH+=../../..
IPATH+=../../../third_party/FreeRTOS/Source/portable/GCC/ARM_CM4F
IPATH+=../../../third_party/FreeRTOS
IPATH+=../../../third_party/FreeRTOS/Source/include
IPATH+=../../../third_party

#
# The default rule, which causes the FreeRTOS example to be built.
#
all: ${COMPILER}
all: ${COMPILER}/out.axf

flash: 
	lm4flash gcc/*.bin

#
# The rule to clean out all the build products.
#
clean:
	@rm -rf ${COMPILER} ${wildcard *~}

#
# The rule to create the target directory.
#
${COMPILER}:
	@mkdir -p ${COMPILER}

#
# Rules for building the FreeRTOS example.
#
${COMPILER}/out.axf: ${COMPILER}/buttons.o
${COMPILER}/out.axf: ${COMPILER}/main.o
${COMPILER}/out.axf: ${COMPILER}/heap_2.o
${COMPILER}/out.axf: ${COMPILER}/led_task.o
${COMPILER}/out.axf: ${COMPILER}/timer.o
${COMPILER}/out.axf: ${COMPILER}/stepper.o
${COMPILER}/out.axf: ${COMPILER}/platform.o
${COMPILER}/out.axf: ${COMPILER}/isqrt.o
${COMPILER}/out.axf: ${COMPILER}/shell_task.o
${COMPILER}/out.axf: ${COMPILER}/list.o
${COMPILER}/out.axf: ${COMPILER}/port.o
${COMPILER}/out.axf: ${COMPILER}/queue.o
${COMPILER}/out.axf: ${COMPILER}/rgb.o
${COMPILER}/out.axf: ${COMPILER}/startup_${COMPILER}.o
${COMPILER}/out.axf: ${COMPILER}/switch_task.o
${COMPILER}/out.axf: ${COMPILER}/tasks.o
${COMPILER}/out.axf: ${COMPILER}/uartstdio.o
${COMPILER}/out.axf: ${COMPILER}/ustdlib.o
${COMPILER}/out.axf: ${ROOT}/driverlib/${COMPILER}-cm4f/libdriver-cm4f.a
${COMPILER}/out.axf: out.ld
SCATTERgcc_out=out.ld
ENTRY_out=ResetISR
CFLAGSgcc=-DTARGET_IS_BLIZZARD_RA1

#
# Include the automatically generated dependency files.
#
ifneq (${MAKECMDGOALS},clean)
-include ${wildcard ${COMPILER}/*.d} __dummy__
endif

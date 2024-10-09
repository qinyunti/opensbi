/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024.
 *
 * Author: 
 */

#ifndef __SERIAL_XXX_UART_H__
#define __SERIAL_XXX_UART_H__

#include <sbi/sbi_types.h>

int wuqi_uart_init(unsigned long base, u32 in_freq, u32 baudrate, u32 reg_shift,
		  u32 reg_width, u32 reg_offset);

#endif

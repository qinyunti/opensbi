/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) .
 *
 * Authors:
 *   
 */

#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/serial/fdt_serial.h>
#include <sbi_utils/serial/wuqi-uart.h>

static int serial_wuqi_init(const void *fdt, int nodeoff,
				const struct fdt_match *match)
{
	int rc;
	struct platform_uart_data uart = { 0 };

	rc = fdt_parse_uart_node(fdt, nodeoff, &uart);
	if (rc)
		return rc;

	return wuqi_uart_init(uart.addr, uart.freq, uart.baud,
			     uart.reg_shift, uart.reg_io_width,
			     uart.reg_offset);
}

static const struct fdt_match serial_wuqi_match[] = {
	{ .compatible = "wuqi,wuqi-uart" },
	{ },
};

struct fdt_serial fdt_serial_wuqi = {
	.match_table = serial_wuqi_match,
	.init = serial_wuqi_init,
};
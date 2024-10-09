/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 .
 *
 * Authors:
 *   
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_domain.h>
#include <sbi_utils/serial/wuqi-uart.h>

/* clang-format off */
#define UART_FIFO_DEPTH     256
#define UART_RBR_OFFSET		0	/* In:  Recieve Buffer Register */
#define UART_THR_OFFSET		0	/* Out: Transmitter Holding Register */
#define UART_IER_OFFSET		1	/* I/O: Interrupt Enable Register */

#define UART_CTRL0          5
#define UART_TICK_REF_ON_MSK (1u<<13)
#define UART_BIT_NUM_MSK    (0x03<<16)
#define UART_BIT_NUM_OFF    16
#define UART_DATA_BITS_MIN  5
#define UART_STOPBIT_NUM_MSK    (0x03<<18)
#define UART_STOPBIT_NUM_OFF    18
#define UART_STOPBIT_1  1
#define UART_STOPBIT_1_5  2
#define UART_STOPBIT_2   3
#define UART_RXFIFO_RST_MSK (1u<<1)
#define UART_TXFIFO_RST_MSK (1u<<2)
#define UART_PARITY_EN_MSK      (1u<<15)
#define UART_PARITY_MSK         (1u<<14)

#define UART_CLK_CFG        7
#define UART_CLK_DIV_MSK    0xFFFFF
#define UART_CLK_DIV_OFF    0
#define UART_CLK_FRAG_MSK   0xF
#define UART_CLK_FRAG_OFF   20
#define UART_BAUD_CFG       8
#define UART_BAUD_CFG_AUTOBAUD_EN_MSK 0x01

#define UART_RAM_CNT        12  /* FIFO CNT */
#define UART_TXFIFO_CNT_MSK 0x3FF800
#define UART_TXFIFO_CNT_OFF 11
#define UART_RXFIFO_CNT_MSK 0x7FF
#define UART_RXFIFO_CNT_OFF 0

#define UART_INT_CLR 2
#define UART_INT_RAW    3
#define UART_INT_RAW_TXDONE_MSK (1u<<9)

/* clang-format on */

static volatile char *wuqi_uart_base;
static u32 wuqi_uart_in_freq;
static u32 wuqi_uart_baudrate;
static u32 wuqi_uart_reg_width;
static u32 wuqi_uart_reg_shift;

static u32 get_reg(u32 num)
{
	u32 offset = num << wuqi_uart_reg_shift;

	if (wuqi_uart_reg_width == 1)
		return readb(wuqi_uart_base + offset);
	else if (wuqi_uart_reg_width == 2)
		return readw(wuqi_uart_base + offset);
	else
		return readl(wuqi_uart_base + offset);
}

static void set_reg(u32 num, u32 val)
{
	u32 offset = num << wuqi_uart_reg_shift;

	if (wuqi_uart_reg_width == 1)
		writeb(val, wuqi_uart_base + offset);
	else if (wuqi_uart_reg_width == 2)
		writew(val, wuqi_uart_base + offset);
	else
		writel(val, wuqi_uart_base + offset);
}

static void wuqi_uart_putc(char ch)
{
	//while ((((get_reg(UART_RAM_CNT) & UART_TXFIFO_CNT_MSK))>>UART_TXFIFO_CNT_OFF) == UART_FIFO_DEPTH)
	//	;
	set_reg(UART_THR_OFFSET, ch);
	while((get_reg(UART_INT_RAW) & UART_INT_RAW_TXDONE_MSK) == 0);
	set_reg(UART_INT_CLR,0x7FFF);  /* clr status */
}

static int wuqi_uart_getc(void)
{
	if ((get_reg(UART_RAM_CNT) & UART_RXFIFO_CNT_MSK)>>UART_RXFIFO_CNT_OFF)
		return get_reg(UART_RBR_OFFSET) & 0xFF;
	return -1;
}

static struct sbi_console_device wuqi_uart_console = {
	.name = "wuqi-uart",
	.console_putc = wuqi_uart_putc,
	.console_getc = wuqi_uart_getc
};

int wuqi_uart_init(unsigned long base, u32 in_freq, u32 baudrate, u32 reg_shift,
		  u32 reg_width, u32 reg_offset)
{
	u32 tmp;
	u32 clock;
	u32 div = 0;
	u32 div_frag = 0;
	wuqi_uart_base      = (volatile char *)base + reg_offset;
	//wuqi_uart_reg_shift = reg_shift;
	wuqi_uart_reg_shift = 2;
	//wuqi_uart_reg_width = reg_width;
	wuqi_uart_reg_width = 0;
	wuqi_uart_in_freq   = in_freq;
	wuqi_uart_baudrate  = baudrate;

	/* Disable all interrupts */
	set_reg(UART_IER_OFFSET, 0x00);

	/* Disable auto baudrate */
	tmp = get_reg(UART_BAUD_CFG);
	tmp &= ~UART_BAUD_CFG_AUTOBAUD_EN_MSK;
	set_reg(UART_BAUD_CFG,tmp);

	/* if baudrate > 115200 select apb clk, else select tick clk*/
	tmp = get_reg(UART_CTRL0);
	if(baudrate > 115200){
		tmp |= UART_TICK_REF_ON_MSK;
		clock = 25000000; /* apb clk */
	}
	else{
		tmp &= ~UART_TICK_REF_ON_MSK; /* tick clk */
		clock = 1000000;
	}
	set_reg(UART_CTRL0,tmp);

	/* set baudrate*/
	if (wuqi_uart_baudrate) {
		div_frag = (clock + (baudrate >> 5)) / (baudrate >> 4) & 0x0F;
		div = clock / baudrate;

		tmp = get_reg(UART_CLK_CFG);
		tmp &= ~(UART_CLK_DIV_MSK | UART_CLK_FRAG_MSK);
		tmp |= (div << UART_CLK_DIV_OFF) | (div_frag << UART_CLK_FRAG_OFF);
		set_reg(UART_CLK_CFG,tmp);
	}

	/* 8 bits, no parity, one stop bit */
	tmp = get_reg(UART_CTRL0);
	tmp &= ~UART_BIT_NUM_MSK;
	tmp |= (8-UART_DATA_BITS_MIN)<<UART_BIT_NUM_OFF;
	tmp &= ~UART_PARITY_EN_MSK;
	tmp &= ~UART_STOPBIT_NUM_MSK;
	tmp |= UART_STOPBIT_1<<UART_STOPBIT_NUM_OFF;
	set_reg(UART_CTRL0,tmp);

	/* reset fifo */
	tmp = get_reg(UART_CTRL0);
	tmp |= (UART_TXFIFO_RST_MSK | UART_RXFIFO_RST_MSK);
	set_reg(UART_CTRL0,tmp);
	tmp &= ~(UART_TXFIFO_RST_MSK | UART_RXFIFO_RST_MSK);
	set_reg(UART_CTRL0,tmp);

	sbi_console_set_device(&wuqi_uart_console);

	return sbi_domain_root_add_memrange(base, PAGE_SIZE, PAGE_SIZE,
					    (SBI_DOMAIN_MEMREGION_MMIO |
					    SBI_DOMAIN_MEMREGION_SHARED_SURW_MRW));
}
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 */

#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <lib/console.h>
#include <lib/debug.h>

#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS	1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS	1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS		0
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS		1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS		0
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS		1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS	1
#define NANOPRINTF_USE_ALT_FORM_FLAG			1
#include <lib/nanoprintf.h>

#define PRINTK_BUFFER_SIZE 256

/* earlycon (usually uart) + fb + pstore-log; bump if a fourth sink appears. */
#define MAX_CONSOLES 3

static const struct console *consoles[MAX_CONSOLES];
static unsigned int n_consoles;

void console_register(const struct console *con)
{
	if (con && n_consoles < MAX_CONSOLES)
		consoles[n_consoles++] = con;
}

static const char *log_prefix(int level)
{
	static const char *const prefix[] = {
		[KERN_EMERG]	= "[EMERG] ",
		[KERN_ALERT]	= "[ALERT] ",
		[KERN_CRIT]	= "[CRIT] ",
		[KERN_ERR]	= "[ERROR] ",
		[KERN_WARNING]	= "[WARN] ",
		[KERN_NOTICE]	= "[NOTICE] ",
		[KERN_INFO]	= "[INFO] ",
		[KERN_DEBUG]	= "[DEBUG] ",
	};

	return (level >= 0 && level < (int)(sizeof(prefix) / sizeof(prefix[0]))) ?
		prefix[level] : "[UNKNOWN] ";
}

#ifdef CONFIG_EARLYCON
extern void uart_puts(const char *s);

static void earlycon_write(int level, const char *prefix, const char *msg)
{
	(void)level; /* unused */
	uart_puts(prefix);
	uart_puts(msg);
	uart_puts("\r");
}

static const struct console earlycon = {
	.write = earlycon_write,
};

void earlycon_register(void)
{
	console_register(&earlycon);
}
#else
void earlycon_register(void) {}
#endif

/*
 * Camellia debug: persist every console line into the vendor's known-safe
 * ramoops console-zone byte range (base + dump-zone size, per the legacy
 * cmdline-supplied ramoops.mem_size=0xe0000/console_size=0x40000 layout
 * ramoops_probe() carves: dump zone [base, base+0x90000), console zone
 * [base+0x90000, base+0xd0000)). Deliberately targets THAT layout, not
 * whatever this specific boot's own kernel might be configured with,
 * because retrieval always happens via a *different*, stock-recovery
 * kernel boot afterward, which only ever knows the legacy cmdline sizes --
 * writing here means it reads back as a real, sig-valid persistent_ram
 * zone (mirrors what fs/pstore/ram_core.c's persistent_ram_buffer expects:
 * u32 sig, u32 start, u32 size, then raw bytes), retrievable the exact
 * same way we already pull kernel-side console-ramoops via adb.
 */
#define PSTORE_LOG_BASE  0x48120000UL
#define PSTORE_LOG_HDR_SIZE 12
#define PSTORE_LOG_MAX   (0x40000 - PSTORE_LOG_HDR_SIZE)
#define PSTORE_LOG_SIG   0x43474244U /* "DBGC", PERSISTENT_RAM_SIG */
#define DCACHE_LINE_SIZE 64UL /* safe assumed size; cleaning extra bytes is harmless */

/*
 * uniLoader never touches SCTLR_EL1 (no MMU/cache-enable code anywhere in
 * this codebase) -- but whatever ran before it (LK/ATF) may leave the MMU
 * and D-cache already enabled, and simply never disables them before
 * jumping here. If so, plain stores to PSTORE_LOG_BASE can sit dirty in
 * cache and never reach physical DRAM before the watchdog reset that
 * follows a hang wipes it -- explaining logged output surviving on-screen
 * (a path that may not go through cacheable memory the same way) while
 * console-ramoops pulls afterward only ever show ancient stale data.
 * Clean-by-VA-to-PoC after every write; a no-op if caches are in fact off.
 */
static void dcache_clean_range(volatile void *start, unsigned long size)
{
	unsigned long addr = (unsigned long)start & ~(DCACHE_LINE_SIZE - 1);
	unsigned long end = (unsigned long)start + size;

	for (; addr < end; addr += DCACHE_LINE_SIZE)
		__asm__ volatile("dc cvac, %0" :: "r" (addr) : "memory");
	__asm__ volatile("dsb sy" ::: "memory");
}

static void pstore_log_write(int level, const char *prefix, const char *msg)
{
	(void)level;
	static int initialized;
	volatile unsigned int *sig  = (volatile unsigned int *)(PSTORE_LOG_BASE + 0);
	volatile unsigned int *start = (volatile unsigned int *)(PSTORE_LOG_BASE + 4);
	volatile unsigned int *size = (volatile unsigned int *)(PSTORE_LOG_BASE + 8);
	volatile char *data = (volatile char *)(PSTORE_LOG_BASE + PSTORE_LOG_HDR_SIZE);
	unsigned int off, start_off;
	const char *s;

	if (!initialized) {
		*sig = PSTORE_LOG_SIG;
		*start = 0;
		*size = 0;
		dcache_clean_range((volatile void *)PSTORE_LOG_BASE, PSTORE_LOG_HDR_SIZE);
		initialized = 1;
	}

	off = *size;
	start_off = off;
	for (s = prefix; *s && off < PSTORE_LOG_MAX; s++, off++)
		data[off] = *s;
	for (s = msg; *s && off < PSTORE_LOG_MAX; s++, off++)
		data[off] = *s;
	*size = off;

	dcache_clean_range((volatile void *)(PSTORE_LOG_BASE + PSTORE_LOG_HDR_SIZE + start_off),
			    off - start_off);
	dcache_clean_range((volatile void *)(PSTORE_LOG_BASE + 8), sizeof(unsigned int)); /* size field */
}

static const struct console pstore_log_console = {
	.write = pstore_log_write,
};

void pstore_log_register(void)
{
	console_register(&pstore_log_console);
}

/* no set-up console for early debugging w/ uart; overridable in board files */
__attribute__((weak)) void early_console_init(void)
{
	earlycon_register();
	pstore_log_register();
}

void printk(int log_level, const char *fmt, ...)
{
	if (log_level > CONFIG_LOGLEVEL || !n_consoles)
		return;

	va_list args;
	static char print_buffer[PRINTK_BUFFER_SIZE];

	va_start(args, fmt);
	npf_vsnprintf(print_buffer, sizeof(print_buffer), fmt, args);
	va_end(args);

	const char *prefix = log_prefix(log_level);

	for (unsigned int i = 0; i < n_consoles; i++)
		consoles[i]->write(log_level, prefix, print_buffer);
}

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 * Copyright (c) 2026, Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <lib/debug.h>
#include <main/boot.h>
#include <string.h>

/*
 * Camellia debug: raw write-then-readback self-test, independent of the
 * pstore_log_console mechanism entirely -- answers "does a write to this
 * physical address even take effect at all" (address/mapping problem)
 * vs. "it takes effect locally but the content doesn't survive whatever
 * reset follows" (both the two prior theories -- D-cache and reset type
 * -- fall under this second case). Deliberately at PSTORE_LOG_BASE+0x39000,
 * far past where the actual tiny log volume could ever reach, so it can't
 * collide with real log text. Result is printed on-screen via the normal
 * printk() sinks, visible immediately, before anything else runs.
 */
#define SELFTEST_ADDR   0x481c9000UL /* PSTORE_LOG_BASE (0x48120000) + 0x39000 */

static void camellia_memtest(void)
{
	volatile unsigned int *p = (volatile unsigned int *)SELFTEST_ADDR;
	unsigned int magic = 0xDEADBEEF;

	*p = magic;
	__asm__ volatile("dsb sy" ::: "memory");
	printk(KERN_INFO, "camellia_dbg: memtest addr=%p wrote=0x%x read=0x%x %s\n",
		(void *)p, magic, *p, (*p == magic) ? "PASS" : "FAIL");
}

void arch_load_kernel(void* kernel, void* dt, void* ramdisk)
{
	camellia_memtest();
	printk(KERN_INFO, "camellia_dbg: kernel copy: src=%p dst=%p size=%lu\n",
		kernel, (void*)CONFIG_PAYLOAD_ENTRY, (unsigned long) &kernel_size);
	memcpy((void*)CONFIG_PAYLOAD_ENTRY, kernel, (unsigned long) &kernel_size);
	printk(KERN_INFO, "camellia_dbg: kernel copy done\n");
#ifndef CONFIG_RAMDISK_NO_COPY
	printk(KERN_INFO, "camellia_dbg: ramdisk copy: src=%p dst=%p size=%lu\n",
		ramdisk, (void*)CONFIG_RAMDISK_ENTRY, (unsigned long) &ramdisk_size);
	__optimized_memcpy((void*)CONFIG_RAMDISK_ENTRY, ramdisk, (unsigned long) &ramdisk_size);
	printk(KERN_INFO, "camellia_dbg: ramdisk copy done\n");
#endif
	printk(KERN_INFO, "camellia_dbg: jumping to kernel at %p, dtb=%p\n",
		(void*)CONFIG_PAYLOAD_ENTRY, dt);
	load_kernel_and_jump(dt, 0, 0, 0, (void*)CONFIG_PAYLOAD_ENTRY);
}

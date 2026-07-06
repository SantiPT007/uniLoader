// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Ivaylo Ivanov <ivo.ivanov.ivanov1@gmail.com>
 * Copyright (c) 2026, Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <lib/debug.h>
#include <main/boot.h>
#include <string.h>

void arch_load_kernel(void* kernel, void* dt, void* ramdisk)
{
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

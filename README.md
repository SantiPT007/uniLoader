# uniLoader

**uniLoader** is a minimalistic loader, capable of booting Linux kernels.
It can be used as an intermediate bootloader, providing a clean booting
environment in case of a forced and buggy bootloader.

---

## Supported Architectures
- ARMv7
- ARMv8

---

## Make Syntax
```bash
make ARCH=$(arch) CROSS_COMPILE=$(toolchain)
```
---

## Building Example
```bash
sudo apt install aarch64-linux-gnu
git clone https://github.com/ivoszbg/uniLoader
cd uniLoader
cp /home/user/linux/arch/arm64/boot/Image blob/Image
cp /home/user/linux/arch/arm64/boot/dts/exynos/exynos8895-dreamlte.dtb blob/dtb
cp /home/user/ramdisk.gz blob/ramdisk
make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- dreamlte_defconfig
make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu-
```

### Build with Clang and LLVM toolchain
```bash
make ARCH=aarch64 LLVM=1 dreamlte_defconfig
make ARCH=aarch64 LLVM=1
```

```bash
make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=/bin dreamlte_defconfig
make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=/bin
```

---

## Camellia debug logging

**This section only applies to the Xiaomi Camellia (Poco M3 Pro 5G) board
(`CONFIG_XIAOMI_CAMELLIA`). No other board has any of this instrumentation.**

Three kinds of logging/diagnostics were added on top of upstream uniLoader
while bringing up a custom Linux kernel on this device, to answer "where
exactly does boot get stuck" without needing a working UART connection:

1. **Kernel-load checkpoint logging** (`arch/aarch64/load-kernel.c`) --
   `printk()` checkpoints before/after copying the embedded kernel blob to
   `CONFIG_PAYLOAD_ENTRY`, before/after copying the ramdisk to
   `CONFIG_RAMDISK_ENTRY`, and right before jumping into the kernel
   (`load_kernel_and_jump()`). Pins down whether a hang is in a memcpy, the
   jump itself, or inside the Linux kernel after control has already left
   uniLoader.

2. **Persistent ramoops-backed console capture** (`lib/console/console.c`) --
   every `printk()` line (splash, board init messages, and the checkpoints
   above) is additionally written as a real `persistent_ram_buffer`-formatted
   entry (matching Linux's `fs/pstore/ram_core.c` layout: `u32 sig` +
   `u32 start` + `u32 size` + raw text) at physical `0x48120000`. That
   address is deliberately the position a *stock/legacy-config* Android
   kernel computes for its own ramoops console zone -- not wherever this
   particular test's own kernel might place its zone -- because retrieval
   always happens through a separate kernel boot afterward (see below), and
   that kernel only ever knows the legacy cmdline-supplied ramoops sizes
   regardless of what actually ran. This means the full, exact text of
   everything uniLoader printed survives a crash/reboot and can be pulled
   with:
   ```
   adb pull /sys/fs/pstore/console-ramoops-2
   ```
   after booting into a (stock) recovery -- no UART, no reading fast-moving
   text off the screen in real time required.

3. **ADB enabled automatically, no manual step** -- `patch-camellia-ramdisk.sh`
   takes a stock ramdisk and flips two `prop.default` lines:
   `persist.sys.usb.config=none` -> `adb`, and `ro.adb.secure=1` -> `0`. The
   first makes the existing `on property:sys.usb.config=adb -> start adbd`
   rule in `init.rc` fire unconditionally at boot instead of waiting for the
   recovery UI's own "Enable ADB" menu toggle to set that property; the
   second skips the RSA key-authorization prompt, so `adb devices` shows the
   phone as `device` immediately instead of `unauthorized`. Use the patched
   output as `blob/ramdisk` (it's still a stock ramdisk otherwise, nothing
   else is touched):
   ```
   ./patch-camellia-ramdisk.sh stock_ramdisk.img blob/ramdisk
   ```

None of this is upstream/generic uniLoader behavior, and none of it is
meant to be -- it's throwaway, device-specific debug tooling for one bring-up
effort, kept separate on purpose.

---

## License
This project is licensed under GPLv2.

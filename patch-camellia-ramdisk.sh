#!/bin/bash
# Camellia debug: patch a stock Android ramdisk (cpio.gz) so adbd starts
# automatically on boot, with no manual "Enable ADB" step and no adb
# authorization prompt. See README.md's "Camellia debug logging" section.
#
# Usage: patch-camellia-ramdisk.sh <stock_ramdisk.img> <out_ramdisk.img>
#
# Patches prop.default:
#   persist.sys.usb.config=none -> adb   (init.rc's
#     `on property:sys.usb.config=adb -> start adbd` then fires unconditionally)
#   ro.adb.secure=1             -> 0     (skips the RSA key-authorization
#     prompt, so `adb devices` shows the phone as `device`, not `unauthorized`)
set -e

IN="$1"
OUT="$2"
[ -z "$IN" ] || [ -z "$OUT" ] && { echo "Usage: $0 <stock_ramdisk.img> <out_ramdisk.img>"; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

zcat "$IN" | (cd "$WORK" && cpio -idm --quiet)

sed -i \
    -e 's/^persist\.sys\.usb\.config=none$/persist.sys.usb.config=adb/' \
    -e 's/^ro\.adb\.secure=1$/ro.adb.secure=0/' \
    "$WORK/prop.default"

(cd "$WORK" && find . | cpio --quiet -o -H newc) | gzip > "$OUT"

echo "wrote $OUT"

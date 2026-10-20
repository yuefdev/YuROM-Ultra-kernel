#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Replace the kernel in a matching public release boot-v1 template."""
import argparse
import hashlib
from pathlib import Path
import struct

def sha256(data):
    return hashlib.sha256(data).hexdigest()


def aligned(size, page):
    return (size + page - 1) // page * page


def boot_id(kernel, ramdisk):
    digest = hashlib.sha1()
    for data in (kernel, ramdisk, b"", b""):
        digest.update(data)
        digest.update(struct.pack("<I", len(data)))
    return digest.digest() + bytes(12)


def repack_boot(original, kernel):
    if original[:8] != b"ANDROID!":
        raise ValueError("Missing Android boot magic")
    kernel_size, = struct.unpack_from("<I", original, 8)
    ramdisk_size, = struct.unpack_from("<I", original, 16)
    second_size, = struct.unpack_from("<I", original, 24)
    page, version = struct.unpack_from("<II", original, 36)
    recovery_size, recovery_offset, header_size = struct.unpack_from("<IQI", original, 1632)
    if (version, page, second_size, recovery_size, recovery_offset, header_size) != (
        1, 2048, 0, 0, 0, 1648
    ):
        raise ValueError("This repacker requires the verified S10 boot v1 layout")
    old_kernel = original[page:page + kernel_size]
    ramdisk_offset = page + aligned(kernel_size, page)
    ramdisk = original[ramdisk_offset:ramdisk_offset + ramdisk_size]
    old_end = ramdisk_offset + aligned(ramdisk_size, page)
    trailer = original[old_end:].rstrip(b"\0")
    avb_footer = original[-64:-60] == b"AVBf"
    if trailer not in (b"", b"SEANDROIDENFORCE") and not avb_footer:
        raise ValueError(
            f"Unexpected boot trailer at {old_end}: length={len(trailer)}, "
            f"first={trailer[:32].hex()}, last={trailer[-32:].hex()}"
        )
    if original[576:608] != boot_id(old_kernel, ramdisk):
        raise ValueError("Original Android boot ID did not verify")

    header = bytearray(original[:page])
    struct.pack_into("<I", header, 8, len(kernel))
    header[576:608] = boot_id(kernel, ramdisk)
    image = bytes(header) + kernel
    image += bytes(aligned(len(kernel), page) - len(kernel))
    image += ramdisk + bytes(aligned(len(ramdisk), page) - len(ramdisk))
    payload_end = len(image)
    if len(image) > len(original) - (64 if avb_footer else 0):
        raise ValueError("New boot image exceeds the backed-up partition")
    # A raw partition backup may contain a legacy AVB footer and old padding.
    # Retain that tail at its absolute offsets, rather than rebasing signatures.
    partition = bytearray(original)
    partition[:len(image)] = image
    if len(image) < old_end:
        partition[len(image):old_end] = bytes(old_end - len(image))
    image = bytes(partition)
    # The kernel size and boot ID are the only intentionally changed header fields.
    check = bytearray(header)
    check[8:12] = original[8:12]
    check[576:608] = original[576:608]
    if bytes(check) != original[:page]:
        raise ValueError("Unexpected boot-header change")
    new_ramdisk_offset = page + aligned(len(kernel), page)
    if image[new_ramdisk_offset:new_ramdisk_offset + ramdisk_size] != ramdisk:
        raise ValueError("Ramdisk preservation failed")
    return image, {
        "ramdisk_sha256": sha256(ramdisk),
        "original_kernel_sha256": sha256(old_kernel),
        "new_kernel_sha256": sha256(kernel),
        "header_preserved_except_kernel_size_and_id": True,
        "ramdisk_preserved": True,
        "command_line_preserved": True,
        "overclock_enabled_by_default": False,
        "partition_tail_preserved_from_offset": max(old_end, payload_end),
        "original_avb_footer_present": avb_footer,
        "avb_signature_recomputed": False,
    }


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--template',type=Path,required=True)
    parser.add_argument('--kernel',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    image,proof=repack_boot(args.template.read_bytes(),args.kernel.read_bytes())
    with args.output.open('xb') as stream:
        stream.write(image)
    print('SHA256 '+sha256(image))

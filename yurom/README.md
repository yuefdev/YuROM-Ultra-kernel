# YuROM Ultra V35 Beta — corresponding kernel source

This tree is the pinned Exynos 9820 kernel at
`a97afb2c431802523a89fb875a3faadb7dcaac26`, with the reviewed performance
patch already applied and its two additional compiled headers included.
The original GPL-2.0 licence, copyright notices and upstream history remain.
`v35-performance.patch` records that change; do not apply it again to this tree.

## Exact release configurations

`configs/SM-G970F.config`, `configs/SM-G973F.config` and
`configs/SM-G975F.config` were extracted from the corresponding distributed boot
images. Each SHA-256 matches its original build manifest. `release-inputs.json`
pins the config, kernel image, boot image, DTB and DTBO hashes for each model.

Use Android Clang r547379 (Clang 20), with the full toolchain Git-tree hash
`a86cb6592a29d53254d3a88ff2d8282233a273e2`. The original verified toolchain
archive SHA-256 is recorded alongside it. The compiler is available from
https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/.
An alternate compiler may build but is not the recorded release toolchain.

## Compile

From this repository root, with `CLANG` pointing to the extracted toolchain:

```sh
MODEL=SM-G973F
mkdir -p ../yurom-kernel-build
cp "yurom/configs/$MODEL.config" ../yurom-kernel-build/.config
export PATH="$CLANG/bin:$PATH"
make O=../yurom-kernel-build ARCH=arm64 LLVM=1 LLVM_IAS=1 \
  CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi- olddefconfig
make O=../yurom-kernel-build ARCH=arm64 LLVM=1 LLVM_IAS=1 \
  CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi- -j8 Image dtbs
```

Select only the config for the target model. Generated timestamps, compiler
paths and build identities can change binary hashes even with matching sources.

## Boot and device-tree packing

The ROM source repository includes `scripts/build_oneui9_s10_kernel.py`, which
documents the exact stock-layout boot header, model DTBO selection, ramdisk FBE
fstab changes, toolchain verification and partition size checks used for family
builds. Use the matching model's HWC1/HWC2 stock input and DT configuration;
do not interchange S10e/S10/S10+ boot or DTBO files.

For the reference S10, the release keeps its existing Android boot-v1 header,
ramdisk, command line and partition tail while replacing the kernel and boot ID.
`repack_boot.py` implements that packing step with the public matching release
boot image as template. It does not access any device or private backup.

The inherited AVB footer is not re-signed. The ROM's matching disabled-AVB
metadata and original partition layout are required. These source instructions
do not claim a new physical clean-install acceptance.

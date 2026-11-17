#!/usr/bin/env python3
"""Build and validate an experimental image; never flash a connected device."""

import hashlib
import json
from pathlib import Path
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]
ARM = ROOT / ".cache/toolchains/xpack-arm-none-eabi-gcc-14.2.1-1.1/bin/arm-none-eabi"
LIB = ROOT / ".cache/vendor/libopencm3-a3977e14519e4cff8d1feb96445655465797908b"
DFU_SUFFIX = ROOT / ".cache/toolchains/dfu-util-0.11/bin/dfu-suffix"


def main():
    build = ROOT / "build"
    build.mkdir(exist_ok=True)
    elf = build / "tangential-knobx1-element-mf.elf"
    binary = build / "tangential-knobx1-element-mf.bin"
    source = ROOT / "src/firmware"
    command = [str(ARM) + "-gcc", "-Os", "-g3", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-mcpu=cortex-m3", "-mthumb", "-msoft-float", "-DSTM32F1",
               "-fno-common", "-ffunction-sections", "-fdata-sections",
               "-I" + str(LIB / "include"), "-I" + str(source),
               "-nostartfiles", "--specs=nano.specs", "--specs=nosys.specs",
               "-L" + str(LIB / "lib"), "-T" + str(source / "knobx1.ld"),
               "-Wl,--gc-sections", "-Wl,-Map=" + str(build / "tangential-knobx1-element-mf.map"),
               str(source / "main.c"), str(source / "underglow.c"),
               "-lopencm3_stm32f1", "-lc", "-lgcc", "-lnosys", "-o", str(elf)]
    subprocess.run(command, check=True)
    subprocess.run([str(ARM) + "-objcopy", "-O", "binary", str(elf), str(binary)], check=True)
    image = binary.read_bytes()
    sp, entry = struct.unpack_from("<II", image)
    if not (0x20000000 <= sp <= 0x20005000 and 0x08002000 <= entry < 0x08010000 and entry & 1):
        raise RuntimeError("Image vectors are outside the intended application/RAM regions")
    if len(image) > 56 * 1024:
        raise RuntimeError("Image exceeds the application region")
    subprocess.run([str(DFU_SUFFIX), "-v", "1eaf", "-p", "0003", "-a", str(binary)], check=True)
    subprocess.run([str(DFU_SUFFIX), "-c", str(binary)], check=True)
    subprocess.run([str(ARM) + "-size", str(elf)], check=True)
    manifest = {
        "status": "Experimental; not yet flashed or verified with Tangent Hub/Flame",
        "usb_identity": "04d8:fa1a", "usb_serial": "99000101",
        "emulated_panel": "Element Mf", "input": "encoder 2 (Jog), button 15 (Play), button 14 (Stop)",
        "required_mapping": "Flame's shipped default profile; no custom map",
        "play_stop_behavior": "Tap large knob for native Play on release; hold 300 ms for native Stop",
        "playback_state_tracking": "None required; gestures send explicit native actions",
        "jog_counts_per_detent": 74,
        "underglow": {"led_count": 2, "driver": "WS2812", "pin": "PC13",
                      "rgb": [2, 2, 2], "behavior": "Steady dim white; set once at startup"},
        "bootloader": "Existing STM32duino; application origin 0x08002000",
        "recovery": "Vendor USB boot request verified on v0.3; cold-start with small button also worked. Runtime small-button hold remains unverified.",
        "development_protocol_revision": 10,
        "image_payload_bytes": len(image), "image_file_bytes": binary.stat().st_size,
        "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "initial_stack_pointer": hex(sp), "reset_vector": hex(entry),
        "compiler": subprocess.check_output([str(ARM) + "-gcc", "--version"], text=True).splitlines()[0],
        "libopencm3_commit": "a3977e14519e4cff8d1feb96445655465797908b",
    }
    (build / "tangential-knobx1-element-mf.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()


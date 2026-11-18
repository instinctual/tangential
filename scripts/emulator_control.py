#!/usr/bin/env python3
"""Read emulator diagnostics or request its existing bootloader. No flashing."""
import argparse
import ctypes
import ctypes.util
import json
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("status", "bootloader"))
    args = parser.parse_args()
    lib = ctypes.CDLL(ctypes.util.find_library("usb-1.0"))
    pointer = ctypes.c_void_p
    lib.libusb_init.argtypes = [ctypes.POINTER(pointer)]
    lib.libusb_init.restype = ctypes.c_int
    lib.libusb_open_device_with_vid_pid.argtypes = [pointer, ctypes.c_uint16, ctypes.c_uint16]
    lib.libusb_open_device_with_vid_pid.restype = pointer
    lib.libusb_get_string_descriptor_ascii.argtypes = [pointer, ctypes.c_uint8, pointer, ctypes.c_int]
    lib.libusb_get_string_descriptor_ascii.restype = ctypes.c_int
    lib.libusb_control_transfer.argtypes = [pointer, ctypes.c_uint8, ctypes.c_uint8,
        ctypes.c_uint16, ctypes.c_uint16, pointer, ctypes.c_uint16, ctypes.c_uint]
    lib.libusb_control_transfer.restype = ctypes.c_int
    lib.libusb_close.argtypes = [pointer]
    lib.libusb_exit.argtypes = [pointer]
    ctx = pointer()
    if lib.libusb_init(ctypes.byref(ctx)) != 0:
        raise RuntimeError("libusb initialization failed")
    handle = None
    try:
        handle = lib.libusb_open_device_with_vid_pid(ctx, 0x04d8, 0xfa1a)
        if not handle:
            raise RuntimeError("No accessible Element-Mf device")
        def string(index):
            buffer = ctypes.create_string_buffer(128)
            size = lib.libusb_get_string_descriptor_ascii(handle, index, buffer, len(buffer))
            if size < 0:
                raise RuntimeError("Cannot identify device")
            return buffer.raw[:size].decode("ascii")
        if string(1) != "Tangential" or string(3) != "99000101":
            raise RuntimeError("Refusing to send development commands to a real Tangent panel")
        if args.command == "status":
            buffer = ctypes.create_string_buffer(12)
            result = lib.libusb_control_transfer(handle, 0xc0, 0x54, 1, 0x5831, buffer, 12, 2000)
            if result != 12:
                raise RuntimeError("Diagnostic request failed: " + str(result))
            a, b, milliseconds, configured, pressed, revision, page = struct.unpack("<HHIBBBB", buffer.raw)
            print(json.dumps({"gpio_a":hex(a),"gpio_b":hex(b),"uptime_ms":milliseconds,
                              "small_button_down":not bool(a & 0x80),"large_button_down":not bool(a & 0x8000),
                              "usb_configured":bool(configured),"debounced_large_button":bool(pressed),
                              "revision":revision,"page":page if revision >= 11 else None},indent=2))
        else:
            result = lib.libusb_control_transfer(handle, 0x40, 0x54, 0x424c, 0x5831, None, 0, 2000)
            if result not in (0, -4):
                raise RuntimeError("Bootloader request failed: " + str(result))
            print("Bootloader request sent; verify 1eaf:0003 before using any DFU tool.")
    finally:
        if handle:
            lib.libusb_close(handle)
        lib.libusb_exit(ctx)


if __name__ == "__main__":
    main()

# Tangential

Public repository: [instinctual/tangential](https://github.com/instinctual/tangential), default branch `main`.

Experimental onboard Tangent panel emulation for a Binepad KnobX1 on Linux. Rotate for native jog, tap and release the large knob for Play, and hold it for 300 ms for Stop. Shuttle is excluded.

The device uses Flame's shipped Tangent profile, with no custom map, keyboard shortcut layer, or workstation bridge. It presents an Element Mf and sends its jog-ring, Play-button, and Stop-button reports. All 37 Mf modes in the installed 2027.2 default map use these controls consistently. The operator confirmed Jog, tap-to-Play, and hold-to-Stop in Flare 2027.2 and Flame 2027.2; captures independently show native Tangent Transport and Play/Stop actions. Revision 10 is installed with the requested 300 ms hold and steady dim-white underglow. The timing passes offline boundary tests, and USB startup was verified after installation. Compatibility on other workstation/application versions remains untested.

Play is sent only after a short release. A hold sends Stop without first sending Play. These explicit gestures require no playback-state tracking. Both bottom RGB LEDs are set to `(2, 2, 2)` out of 255 per channel, under 1% PWM level. Levels 12 and 3 were too bright; revision 10 restores the previous dimmer level 2. Their color is written once at startup and remains steady.

An earlier Element Kb prototype established USB/Hub recognition and working rotation and large-button input. It is superseded because the default Kb profile maps those controls to grading parameters. The original KnobX1 application has been read back twice and preserved. See [HANDOFF.md](HANDOFF.md) for the installed firmware and test state.

The installed v0.10 [firmware snapshot](artifacts/firmware-candidates/element-mf-v0.10/) and [installation record](artifacts/hardware-test-element-mf-v0.10/device.json) are committed with the project. The [v0.5 transport verification](artifacts/hardware-test-element-mf-v0.5/verification.json) records the operator-confirmed controls and native Tangent traffic. Build manifests retain their original pre-flash status; installation records document subsequent hardware results.

## Build and test

Setup requires Linux x86_64, Python 3.12+, a native C compiler, Make, pkg-config, and libusb development headers. The hardware inspection CLI itself uses only Python's standard library.

```bash
make setup
make test
make firmware
```

`make setup` downloads checksum-pinned ARM compiler, USB-library, and DFU-tool sources into `.cache/` and builds the local dependencies. It installs no system packages. Sources and checksums are in [build-lock.json](build-lock.json).

`make test` runs Python protocol/command tests and native C tests for report encoding, jog scaling, quadrature direction, tap/hold boundaries, button-release handling, descriptors, and USB reconfiguration callbacks. Recorded USB traffic is used as an offline fixture.

`make firmware` produces `build/tangential-knobx1-element-mf.{bin,elf,json}`. It checks the application vectors, size, and DFU suffix. The linker reserves the existing bootloader at `0x08000000–0x08001fff`; the application starts at `0x08002000`. Build commands do not flash hardware.

## Inspect and capture

```bash
python3 scripts/inspect_knobx1.py inspect --output artifacts/baseline-NEW.json
python3 scripts/inspect_knobx1.py monitor --seconds 120 --output artifacts/inputs-NEW.jsonl
python3 scripts/summarize_usb_capture.py CAPTURE.pcap --output artifacts/decoded-NEW.json
```

Use fresh output filenames. The KnobX1 inspector applies to the original VIA/Vial firmware and permits only documented getters. Tangent Hub claims panel interfaces, so use passive Linux `usbmon` captures to observe their traffic. Select only the intended bus/device and coordinate movements on a disposable Flame setup. `scripts/summarize_tipc_capture.py` decodes captured native Hub/application traffic offline; it does not send controls.

## Firmware entry and recovery

`scripts/enter_knobx1_bootloader.py` is a separate state-changing tool: it uses Vial's physical-unlock flow and then requests the existing bootloader. It checks the saved keyboard UID and requires the operator to hold the large knob. It has no flash-write, EEPROM-reset, or remapping command. A timed-out unlock may require a normal unplug/reconnect to cancel the volatile state.

The bootloader identifies as `1eaf:0003`; alternate setting 2 addresses `0x08002000`. The original application/settings bytes are preserved under `artifacts/research/firmware/`. Both independent 120 KiB reads match, but restoring them has not yet been tested. The bootloader itself is outside the backup.

The firmware has guarded vendor USB diagnostics and a bootloader request, accessed through `python3 scripts/emulator_control.py status` or `bootloader`. The helper checks the emulator manufacturer and serial before issuing a request and never flashes. Bootloader entry through this helper has worked repeatedly. Disconnecting USB at the knob for five seconds and reconnecting while holding the small button also worked; runtime small-button hold remains unverified. The first indicator LED shows USB configuration; panel display commands are accepted and discarded.

Tangent Hub can claim the emulator without a service restart. In this session, restarting Hub left Flare's old TCP connection in CLOSE-WAIT; the operator had to restart Flare. Plan any further Hub restart accordingly.

## Tangent connection troubleshooting

If the knob responds to `emulator_control.py status` but Flame ignores it, inspect `sudo ss -tnp '( sport = :64246 or dport = :64246 )'`. Autodesk background processes can retain an abandoned application's Tangent socket after exit or logout, leaving Hub routing controls to the old session. In the [recorded incident](artifacts/diagnostics/flame-connection-20261117T025606Z/resolution.json), closing the confirmed abandoned connection restored all three controls in Flame without restarting Hub or changing firmware. Identify current process owners before closing a connection; recorded ports are historical. See `HANDOFF.md` for the evidence and recovery details.

## Layout

- `src/firmware/`: STM32 application and linker script.
- `src/tangential/`: offline report and usbmon parsers.
- `scripts/`: inspection, build, setup, and bootloader-entry tools.
- `tests/`: offline Python and C tests.
- `artifacts/`: captures, baselines, recovery files, and vendor research.

The firmware links [libopencm3](https://github.com/libopencm3/libopencm3); its GPIO assignments follow the published [QMK KnobX1 definition](https://github.com/qmk/qmk_firmware/tree/master/keyboards/binepad/knobx1). Vendor source archives retain their original licenses.


# Tangential

Public repository: [instinctual/tangential](https://github.com/instinctual/tangential), default branch `main`.

Experimental onboard Tangent panel emulation for a Binepad KnobX1 on Linux. Firmware v0.11 is installed and its four pages have passed operator testing in Flare. Briefly press/release the small button to cycle pages; one indicator identifies the selected page.

| Page | Behavior | Native Mf control |
| --- | --- | --- |
| 1 | Jog | Outer ring |
| 2 | Jog, reversed orientation | Outer ring |
| 3 | Faster scrubbing | Trackball X, labelled Shuttle |
| 4 | Faster scrubbing, reversed orientation | Trackball X, labelled Shuttle |

On every page, tap/release the large knob for Play and hold it for 300 ms for Stop. Power-on starts at page 1; USB reconfiguration preserves the selected page. Page changes discard buffered motion from the previous page.

Pages 3/4 reproduce the real Mf's Shuttle-labelled control. With the shipped profile, it sends larger Jog deltas and movement stops when the trackball stops. It does **not** provide continuous playback-speed control. After comparing the real panel, the user chose to keep this behavior and defer further Shuttle changes. See the [operator acceptance](artifacts/hardware-test-element-mf-v0.11/operator-acceptance.json) and [real Mf observations](artifacts/element-mf-20261118T201737Z/live-behavior/conclusion.json).

The device uses Flame's shipped Tangent profile, with no custom map, keyboard shortcut layer, or workstation bridge. All 37 Mf modes in the inspected 2027.2 default map use these controls consistently. Captures independently show native Tangent Transport and Play/Stop actions. Four-page testing used Flare `2027.2.case26507689`; earlier transport tests used Flame and Flare 2027.2. Compatibility on other workstations/application versions remains untested.

Play is sent only after a short release. A hold sends Stop without first sending Play. These explicit gestures require no playback-state tracking. Both bottom RGB LEDs are set to `(2, 2, 2)` out of 255 per channel, under 1% PWM level. Levels 12 and 3 were too bright. Their color is written once at startup and remains steady.

An earlier Element Kb prototype established USB/Hub recognition and working rotation and large-button input. It is superseded because the default Kb profile maps those controls to grading parameters. The original KnobX1 application has been read back twice and preserved. See [HANDOFF.md](HANDOFF.md) for the installed firmware and test state.

The current v0.11 [firmware snapshot](artifacts/firmware-candidates/element-mf-v0.11/) and [installation record](artifacts/hardware-test-element-mf-v0.11/device.json) are preserved. The previous [v0.10 image](artifacts/firmware-candidates/element-mf-v0.10/) remains available. The [v0.5 transport verification](artifacts/hardware-test-element-mf-v0.5/verification.json) records the earlier operator-confirmed controls and native Tangent traffic. Build manifests retain their original pre-flash status; installation records document subsequent hardware results.

The four-page implementation and its hardware evidence were published in [commit `60e5d67`](https://github.com/instinctual/tangential/commit/60e5d67dad6c6e8bc4147c1b1ac30897f9a55f7e). The [reconnection check](artifacts/hardware-test-element-mf-v0.11/reconnected.json) confirms the final installed revision and page 1 after reconnecting.

## Build and test

Setup requires Linux x86_64, Python 3.12+, a native C compiler, Make, pkg-config, and libusb development headers. The hardware inspection CLI itself uses only Python's standard library.

```bash
make setup
make test
make firmware
```

`make setup` downloads checksum-pinned ARM compiler, USB-library, and DFU-tool sources into `.cache/` and builds the local dependencies. It installs no system packages. Sources and checksums are in [build-lock.json](build-lock.json).

`make test` runs Python protocol/command tests and native C tests for report encoding, jog scaling, quadrature direction, tap/hold boundaries, page selection and reversal, indicators, button-release handling, descriptors, and USB reconfiguration. Recorded Kb and real Mf traffic is used as offline fixtures.

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

The firmware has guarded vendor USB diagnostics and a bootloader request, accessed through `python3 scripts/emulator_control.py status` or `bootloader`. Diagnostics include the current page on revision 11. The helper checks the emulator manufacturer and serial before issuing a request and never flashes. Bootloader entry through this helper has worked repeatedly. Disconnecting USB at the knob for five seconds and reconnecting while holding the small button also worked; runtime small-button hold remains unverified. A short small-button release changes page; a two-second hold requests the bootloader. Page indicators illuminate when USB is configured; panel display commands are accepted and discarded.

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


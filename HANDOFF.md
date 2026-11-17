# KnobX1 Tangent Emulation Handoff

Updated 2026-11-16 during firmware development. Element Mf v0.10 is installed on the KnobX1. The operator confirmed native Jog, tap-to-Play, and hold-to-Stop on v0.5. At the user's request, v0.6 shortened the hold from 600 to 300 ms. v0.7 added steady white bottom lighting at RGB 12, which the user found too bright; v0.8 lowered it to 2, and the user requested a small increase. v0.9 set RGB 3, also reported too bright. v0.10 restores level 2. The timing passes offline boundary tests and runtime revision 10 is verified. No custom map was created or installed. All application control uses emulated Tangent USB reports through the standard Hub and Flame's native Tangent integration; the user explicitly rejects other Flame APIs or shortcuts as substitutes.

## Current development state

- Workspace: `/vol/engineering/dev/tangential`. The old directory name below belongs to the initial inspection history.
- User approved installing experimental onboard firmware. Original application/settings flash was read twice into `artifacts/research/firmware/knobx1-original-app-read{1,2}.bin`; both 122,880-byte reads match SHA256 `a03fd94c015aa069ed80102acd57c9a7c2f316d58d8a2eafb2925ca942abca90`. Both tools stalled on the extra end-of-region read; the complete returned bytes match. The first 8 KiB bootloader is excluded; restore has not been tested.
- Existing STM32duino bootloader verified as `1eaf:0003`; alternate setting 2 targets `0x08002000`. Vial's physical unlock plus bootloader request worked without changing mappings. `scripts/enter_knobx1_bootloader.py` applies only to the original Vial firmware.
- Element Kb v0.1 image SHA256 `9a0e2fc183a903a250e02a842918de92dd3c4f0db0d1739462a8b684df92a0d7` was installed and enumerates as `04d8:fa1e`, manufacturer `Tangential`, serial `99000101`. Hub sent Flare's labels to it. Physical input evidence is in `artifacts/hardware-test-element-kb-v0.1/physical-input-check.json`.
- The v0.1 small-button runtime hold did not enter DFU. GPIO assignments match the original application disassembly: PA7 and PA15 inputs, PB8 switch row low, PB12/PB13 encoder phases. Disconnecting the cable at the knob for five seconds and reconnecting while holding the small button worked. Runtime hold remains unverified.
- Mf v0.2 was built but not installed. Mf v0.3 was installed after cold-start DFU entry; SHA256 `48a86dcd3ea7294c70b3ac1772b4afc330afd6dd744dc0c16255f07708dae646`. Its guarded vendor USB boot request successfully entered `1eaf:0003` without operator action for the v0.4 update. Use `python3 scripts/emulator_control.py status|bootloader`; this helper never sends application controls.
- Mf v0.10 is installed: SHA256 `9bc559fbe7908e00812df3a25a7d7aef9d9e6a21a39e617962f15a4aa33e0879`, 7936 application bytes. Current path `5-5.4.4`, USB bus 5 device 31, identity `04d8:fa1a`, serial `99000101`, diagnostics revision 10. DFU log says download complete and status 0, with exit 74 after reset; successful runtime enumeration and revision were verified. The resident bootloader remains untouched. Source/image snapshots are in `artifacts/firmware-candidates/element-mf-v0.10/`.
- Mf reports use encoder 2 (byte 5) for Jog, button 15 (byte 1, bit 7) for Play, and button 14 (byte 1, bit 6) for Stop. Shipped-map assignments are saved in `artifacts/research/firmware/flame-default-mf-transport.json`; 37 modes were checked. Native Play does not toggle Stop, and no playback feedback appeared in the captured HighlightControl messages. The user chose explicit tap-to-Play and hold-to-Stop gestures instead of alternating presses. A long hold never sends Play first; a tap sends a Play down/up pair on release. No playback-state tracking is required.
- Hardware evidence: v0.4 produced 467 native Transport events with shuttle zero and fixed the packet-memory overlap (493 clean input reports). On v0.5 the operator confirmed all controls; capture shows 109 Transport events, five native Play down/up pairs, and two native Stop down/up pairs. See `artifacts/hardware-test-element-mf-v0.5/verification.json`. Captures were stopped cleanly with no kernel drops. v0.6 only changes the hold threshold and revision number.
- Read-only Hub analysis confirms the Mf encoder offset. It normalizes the ring at 1180 counts/revolution and 16 jog steps/revolution with default sensitivity 1. Revision 4 sends 74 raw counts per detent (approximately one jog step), versus 1 previously. It also allocates 16 bytes for the 15-byte IN report to avoid overlapping the OUT packet-memory buffer. Evidence: `artifacts/research/firmware/mf-jog-normalization.json`.
- `make setup`, `make test`, and `make firmware` are available. Latest checks: 20 Python tests and two native C test programs passed. ARM builds and DFU suffixes pass. Dependencies are pinned in `build-lock.json` and cached locally.
- Real Element Kb was connected for protocol capture, then unplugged for the emulator test. User has Kb/Bt/Tk available today and can bring an Mf tomorrow. Captures and decoded summaries are in `artifacts/element-kb-20261116/`.
- Running application is Autodesk Flare 2027.2. A captured ApplicationDefinition names `Autodesk Flare`, system maps `/opt/Autodesk/presets/2027.2/tangentPanel`, and an empty user-map directory. Hub's default panel-list expects one of each panel type and has not been edited.
- Tangent Hub was restarted once; Flare did not reconnect automatically (old socket CLOSE-WAIT). User saved/restarted Flare and reconnection was captured. Avoid further Hub restarts unless planned with the operator. Mapper was opened but no mapping was changed; its window was gone when cleanup was attempted.
- User requested only steady white bottom lighting at low intensity. The pinned QMK `keyboards/binepad/knobx1/keyboard.json` specifies two WS2812 LEDs on PC13. `underglow.c` writes `(2,2,2)` once at startup (under 1% PWM level), with no animation. A 72 MHz DWT counter schedules 800 kHz data pulses before USB initialization; unsupported cycle counting exits safely. The compiled pulse loop was inspected. The user found level 12 too bright, requested a small increase from 2, then reported level 3 too bright. v0.10 restores level 2. No further confirmation request is pending. This lighting does not represent playback state.

## Intended behavior

Use the KnobX1 with Flame's native Tangent integration:

- Rotate to jog forward/backward. Shuttle is explicitly unwanted.
- Tap and release the large knob for Play; hold for 300 ms for Stop. This explicitly supersedes the earlier request for alternating large-knob presses.
- Holding the knob while rotating for faster jogging is optional. The user accepted losing this gesture.
- The preferred implementation runs on the knob itself, with no external USB adapter or workstation bridge.
- Use Flame's shipped default profile. A custom transport profile is explicitly rejected.
- Both bottom RGB LEDs should remain steady white at low intensity.

The earlier iWit investigation led to purchasing this KnobX1. Other hardware recommendations are no longer the active task. The working directory is `/vol/engineering/dev/tangential`; the earlier Flame repository is separate. `AGENTS.md` contains the current contributor guide.

## Original device baseline

| Item | Observed value |
| --- | --- |
| USB identity | Binepad KnobX1, `5831:4249` |
| USB serial string | `vial:f64c2b3c` |
| USB device revision | `1.00`; this is not an identified firmware release |
| Connection | Full speed, 12 Mbps; bus 005, device 007 at inspection |
| Sysfs location | `/sys/bus/usb/devices/5-5.4.4` |
| Configuration endpoint | `/dev/hidraw5`, interface 1, vendor HID usage page `0xff60` |
| Input endpoints | `/dev/hidraw2` keyboard; `/dev/hidraw6` mouse/system/consumer reports |
| Additional interfaces | USB MIDI |
| Protocol versions | VIA 9, Vial 6 |
| Vial keyboard UID | `d453bc594b3022fa` |
| Matrix and layers | One row, two columns, five programmable layers |

Device paths can change after reconnecting. The inspection script discovers them by identity and report descriptor. The USB power descriptor requests 500 mA; actual consumption was not measured.

The device accepted 31 getter exchanges. Layer 0 maps clockwise to volume up (`0x00a9`), counterclockwise to volume down (`0x00aa`), knob press to mute (`0x00a8`), and the small button to the custom Layer Jump keycode (`0x7e02`). Layers 1–4 contain transparent mappings (`0x0001`). Small-button behavior was read from the definition, not physically tested.

The operator moved and pressed the knob during capture. Observed consumer HID reports were `04e900` for volume up, `04ea00` for volume down, `04e200` for mute press, and `040000` for release. Capture contained four up events, three down events, one mute event, and eight release reports. The mute press lasted approximately 236 ms. These counts are not a calibrated events-per-detent measurement.

## Saved evidence and tools

- [Inspection results](artifacts/ws104-knobx1-check-20261116.json)
- [USB descriptors, Vial definition, mappings, and query transcript](artifacts/ws104-knobx1-baseline-20261116.json)
- [Live input capture](artifacts/ws104-knobx1-inputs-20261116.jsonl)
- [Read-only inspection and monitoring CLI](scripts/inspect_knobx1.py)
- [Preserved Tangent SDK reference files and hashes](artifacts/research/tangent/manifest.json)

The baseline is a configuration snapshot, not a firmware image backup. The SDK archive, searchable text, and vendor sample are preserved under `artifacts/research/tangent/`; the original copies in `/tmp` are no longer the only copies.

The Python CLI uses only the standard library. Its command allowlist permits configuration getters; it has no flashing, reset, unlock, or remapping operation. Examples from the repository root:

```bash
python3 scripts/inspect_knobx1.py --help
python3 scripts/inspect_knobx1.py inspect --output artifacts/baseline-NEW.json
python3 scripts/inspect_knobx1.py monitor --seconds 120 --output artifacts/inputs-NEW.jsonl
```

Use new output filenames. Hardware access required execution outside the sandbox because `/dev/hidraw*` nodes were hidden inside it. On the host, the HID nodes were accessible to the session user; evdev nodes were not. No permission rules were changed. Monitoring is passive and leaves the knob's normal volume/mute behavior active. Arrange operator movements while capture is running.

## Firmware development evidence

The published [QMK KnobX1 definition](https://github.com/qmk/qmk_firmware/tree/master/keyboards/binepad/knobx1) identifies an STM32F103 and STM32duino bootloader. Its documented bootloader gesture is holding the small layer button while connecting USB. The published VID/PID matches the attached unit. This supports a custom firmware path, but the chip, bootloader, and flashing procedure have not been independently verified on this unit.

The [Vial QMK KnobX1 directory](https://github.com/vial-kb/vial-qmk/tree/vial/keyboards/binepad/knobx1) also contains board code. The checked branch exposed a default keymap, but a matching stock Vial build and recovery image were not located. Do not assume the upstream default reproduces the installed firmware exactly. `dfu-util`, `arm-none-eabi-gcc`, `qmk`, and `openocd` were absent from PATH at inspection.

## Tangent and Flame protocol findings

Onboard emulation reproduces a physical Tangent panel's USB behavior so Tangent Hub recognizes it. Kb USB descriptors and inputs were captured from the real panel; Mf identity and encoder layout were checked against the installed Hub. USB recognition and native Play are verified. Full required transport behavior is not yet verified.

The saved TUBE 3.12 / TIPC revision 9 documentation describes the separate Hub-to-application protocol. The Hub listens on TCP port `64246`, and the application connects to it. Relevant messages include Transport (`0x0A`, signed jog/shuttle values), ActionOn (`0x08`), and ActionOff (`0x0B`). ActionOff represents release/cancellation of an action; it is not a universal playback Stop command.

The local `/opt/Autodesk/presets/2027.2/tangentPanel/controls.xml` enables jog, disables shuttle, and sets jog sensitivity to standard 1 / alternate 5. Relevant action IDs:

| Action | ID |
| --- | --- |
| Play | `0x20000089` |
| Stop Play/Render | `0x20000000` |
| Play Backward | `0x20000090` |
| Next Frame | `0x20001147` |
| Previous Frame | `0x20001148` |

Native Play was verified not to toggle Stop. No usable playback feedback appeared in the captured TIPC traffic. The user chose tap-to-Play and hold-to-Stop, which send explicit native actions and avoid synchronization. Both actions were verified in Flare and in native captures. Generic control feedback in TIPC alone does not establish that Flame supplies playback state.

The previous workstation-bridge concept would read knob events and provide the Hub-facing TCP endpoint expected by Flame. That would require handling the existing Hub listener and any real panels. It is a fallback concept, not implemented here and not the preferred onboard solution.

## Follow-up verification

1. If the user wants further adjustment, use the installed 300 ms hold and RGB `(2,2,2)` as the baseline. No input request or capture monitor is pending/running. Timing unit tests and runtime revision checks passed; the restored brightness has not received a new operator confirmation.
2. Portability beyond this Flare 2027.2 setup, coexistence with a real Mf, and restoring the original firmware remain untested. Compare with the real Mf when available; do not claim universal compatibility.
3. Keep underglow steady dim white; do not add animations or playback-state indication unless requested.

The original baseline above is historical evidence. The active firmware/build/test state is recorded at the top of this handoff and in the candidate and hardware-test artifact directories. USB/Hub recognition, native Jog, tap-to-Play and hold-to-Stop are verified with the shipped default Mf profile in this Flare setup.

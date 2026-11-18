# KnobX1 Tangent Emulation Handoff

## Verified four-page firmware — 2026-11-18

The final decision is to keep v0.11: normal/reversed Jog on pages 1/2 and normal/reversed Mf-style faster scrubbing on pages 3/4. The user confirmed all page indicators, reversals, and Play/Stop behavior, then clarified that true Shuttle should vary ongoing playback speed. A live real-Mf comparison showed movement stops with the trackball. Repeated native Play might increase playback rate, but the user could not confirm it confidently and explicitly chose to leave the current behavior in place. **Do not resume continuous-speed Shuttle work unless requested.** All captures and monitoring processes are stopped. The user has reconnected KnobX1; revision 11 and page 1 were verified again.

The user connected a real Element Mf, serial `55024780`, at USB path `1-1`, bus 1 device 2. Its 100-byte device/configuration descriptors exactly match the emulator snapshot; its 27-byte HID descriptor also matches. The first physical capture confirms ring encoder 2 at byte 5, Play button 15, and Stop button 14. Evidence is in `artifacts/element-mf-20261118T201737Z/`. Approximate clockwise/counterclockwise turns produced 1833 positive and 1513 negative raw counts; do not treat these as a calibrated revolution measurement. Hub emitted native Jog and Play/Stop messages.

The user disconnected KnobX1 for the capture. Hub then opened the real Mf automatically. The current `panel-list.xml` pins Mf and Kb slots to serial `99000101` (different from the older historical snapshot); no host configuration was edited during this inspection. No live Flame/Flare process was running. Hub's TCP client port `58828` was retained by Autodesk background processes, so this validates Hub translation, not current application response.

The user added a four-page requirement, superseding the earlier Jog-only request:

| Page | Native Mf control | Orientation |
| --- | --- | --- |
| 1 | Jog ring, encoder 2 | Normal |
| 2 | Jog ring, encoder 2 | Reversed |
| 3 | Trackball X labelled Shuttle, encoder 0 | Normal |
| 4 | Trackball X labelled Shuttle, encoder 0 | Reversed |

The small button cycles pages on short release. One indicator identifies the selected page. Large-knob tap/release remains Play; hold 300 ms remains Stop. Bottom LEDs remain RGB `(2,2,2)`. Power-on starts on page 1; USB reconfiguration retains the selected page. A two-second small-button hold remains the separate bootloader gesture. Page changes discard unsent motion to prevent sending it through the wrong control/orientation.

Source revision 11 implements these pages. `make test` passes 24 Python tests and two native C programs; the ARM image builds. **v0.11 is installed and all four pages have operator confirmation.** The candidate is preserved in `artifacts/firmware-candidates/element-mf-v0.11/`; installation and test evidence is in `artifacts/hardware-test-element-mf-v0.11/`. Image SHA256 is `9b422e19e642746201313b41129174da16c791d44403078b063c3f28e6514a61`, payload 8080 bytes. The knob is back at `5-5.4.4`, last observed bus 5 device 12. Only the emulator was present at the final identity check; the real Mf is disconnected.

Important Shuttle distinction: the shipped `controls.xml` has `Shuttle enabled="false"`, but all 37 Mf modes map trackball X to reserved transport control `0x81000001`, with `CustomName=Shuttle` and sensitivity 5. The ring uses the same reserved control with `CustomName=Jog` and standard sensitivity. The completed real-Mf trackball capture confirms 12 native Transport messages with the Jog field populated and Shuttle field zero, plus a native Stop down/up pair. The new pages reproduce that labelled Mf control, not a separate continuous-speed Shuttle API. The user was told this before installation. Evidence: `shuttle-inputs-03.pcap`, `shuttle-tipc-03.pcap`, decoded files, and `shuttle-verification.json`. There was minor incidental vertical-ball and ring motion. All real-panel captures are stopped.

Before installation, the confirmed obsolete client connection on port `58828` was closed; no application owned it. Hub reset the knob's USB connection during that cleanup, interrupting the first bootloader identity read before any write. Identity and revision 10 were rechecked, then the guarded boot request and DFU installation succeeded. Revision 11, USB configuration, and page 1 were verified afterward. No Hub restart or profile edit was used.

Live testing used Flare `2027.2.case26507689`. The user reported "All four pages work." The read-only monitor recorded pages `1 → 2 → 3 → 4 → 1`; USB capture contains ring and horizontal-trackball deltas in both directions, and TIPC shows native Play/Stop actions. See `verification.json` for the initial result and subsequent Shuttle clarification, and `operator-acceptance.json` for the final decision to retain this behavior. A later real-Mf comparison is preserved under `artifacts/element-mf-20261118T201737Z/live-behavior/`. That comparison does not verify repeated Play acceleration. No further firmware or control changes followed the user's decision.

## Previous verified installation

Updated 2026-11-16 after troubleshooting the Flame session. Element Mf v0.10 is installed on the KnobX1. The operator confirmed Jog, tap-to-Play, and hold-to-Stop in both Flare 2027.2 (v0.5) and Flame 2027.2 (v0.10). The current Stop hold is 300 ms. Bottom lighting is steady white at RGB `(2,2,2)`; levels 12 and 3 were too bright. A stale Flare TCP connection caused the latest apparent failure; closing that connection restored all controls in Flame without changing firmware or profiles. All application control uses emulated Tangent USB reports through the standard Hub and Flame's shipped profile.

## Resolved Flame connection failure

After the application/session change, the knob still responded with revision 10 and Tangent Hub held its USB interface. Hub had an old Flare connection on client port `58486` retained by Autodesk background processes, plus a new Flame connection on port `37596`. A passive capture showed 58 native Jog messages sent to the abandoned Flare connection and none to current Flame. Logout/login did not remove the inherited old socket.

Confirmed that no live Flame/Flare process owned the old client socket, then closed only that loopback TCP connection with `ss -K`. Hub remained running and the current Flame connection stayed established. The next capture showed 41 native Jog messages (shuttle zero), one Play down/up pair, and one Stop down/up pair reaching Flame. The operator answered "All three work now."

Evidence: [resolution](artifacts/diagnostics/flame-connection-20261117T025606Z/resolution.json), before/after TIPC captures, and USB capture in the same directory. All diagnostic captures were stopped with zero kernel drops. No firmware or profile was changed. Last observed knob address was bus 5 device 37; identity remains `04d8:fa1a`, serial `99000101`.

If this recurs, inspect `sudo ss -tnp '( sport = :64246 or dport = :64246 )'` and current process ownership before taking action. Ports and PIDs above are historical, not reusable targets. Background Autodesk helpers can retain a socket after its parent application exits. Do not kill those services or restart Hub as the first response; the specific abandoned Tangent socket can be removed once distinguished from the live application's connection. Automatic prevention of inherited sockets has not been implemented.

## Repository checkpoint

- Public GitHub repository: [instinctual/tangential](https://github.com/instinctual/tangential). The owner changed its visibility to public after initial publication; this was confirmed through the GitHub API.
- Local branch `main` tracks `origin/main`; remote URL is `https://github.com/instinctual/tangential.git`.
- Published four-page implementation and hardware evidence: [`60e5d67dad6c6e8bc4147c1b1ac30897f9a55f7e`](https://github.com/instinctual/tangential/commit/60e5d67dad6c6e8bc4147c1b1ac30897f9a55f7e), `Add four native transport pages`. Local and remote heads were verified after publication.
- Final acceptance is recorded in [operator-acceptance.json](artifacts/hardware-test-element-mf-v0.11/operator-acceptance.json). The subsequent [reconnection record](artifacts/hardware-test-element-mf-v0.11/reconnected.json) confirms revision 11, page 1, and configured USB. These are the latest saved hardware observations; this documentation checkpoint makes no device changes.
- Initial project commit: [`3a1e51e24894b2107e38fdc1486b7835e096ddc8`](https://github.com/instinctual/tangential/commit/3a1e51e24894b2107e38fdc1486b7835e096ddc8), `Add KnobX1 Tangent transport firmware`. It was pushed and its remote SHA verified.
- Source, contributor instructions, build lock, tests, firmware snapshots, hardware captures, and both original application readbacks are committed. `.cache/`, `.venv/`, and `build/` remain ignored. `.gitattributes` preserves artifact bytes; all 141 archived evidence files and both original firmware checksums were checked before the initial commit.
- Use `git log -1` for the newest documentation commit; the initial commit above is a fixed historical checkpoint. These notes follow that publication and make no additional firmware changes.

## Earlier development state — 2026-11-16

The entries below preserve the earlier v0.10 checkpoint. The verified v0.11 state and final decisions are at the top of this file.

- Workspace: `/vol/engineering/dev/tangential` on `ws104`.
- User approved installing experimental onboard firmware. Original application/settings flash was read twice into `artifacts/research/firmware/knobx1-original-app-read{1,2}.bin`; both 122,880-byte reads match SHA256 `a03fd94c015aa069ed80102acd57c9a7c2f316d58d8a2eafb2925ca942abca90`. Both tools stalled on the extra end-of-region read; the complete returned bytes match. The first 8 KiB bootloader is excluded; restore has not been tested.
- Existing STM32duino bootloader verified as `1eaf:0003`; alternate setting 2 targets `0x08002000`. Vial's physical unlock plus bootloader request worked without changing mappings. `scripts/enter_knobx1_bootloader.py` applies only to the original Vial firmware.
- Element Kb v0.1 image SHA256 `9a0e2fc183a903a250e02a842918de92dd3c4f0db0d1739462a8b684df92a0d7` was installed and enumerates as `04d8:fa1e`, manufacturer `Tangential`, serial `99000101`. Hub sent Flare's labels to it. Physical input evidence is in `artifacts/hardware-test-element-kb-v0.1/physical-input-check.json`.
- The v0.1 small-button runtime hold did not enter DFU. GPIO assignments match the original application disassembly: PA7 and PA15 inputs, PB8 switch row low, PB12/PB13 encoder phases. Disconnecting the cable at the knob for five seconds and reconnecting while holding the small button worked. Runtime hold remains unverified.
- Mf v0.2 was built but not installed. Mf v0.3 was installed after cold-start DFU entry; SHA256 `48a86dcd3ea7294c70b3ac1772b4afc330afd6dd744dc0c16255f07708dae646`. Its guarded vendor USB boot request successfully entered `1eaf:0003` without operator action for the v0.4 update. Use `python3 scripts/emulator_control.py status|bootloader`; this helper never sends application controls.
- At that checkpoint, Mf v0.10 was installed: SHA256 `9bc559fbe7908e00812df3a25a7d7aef9d9e6a21a39e617962f15a4aa33e0879`, 7936 application bytes. The observed path was `5-5.4.4`, USB bus 5 device 31, identity `04d8:fa1a`, serial `99000101`, diagnostics revision 10. USB device numbers can change after reconnecting. DFU log says download complete and status 0, with exit 74 after reset; successful runtime enumeration and revision were verified. The resident bootloader was untouched. Source/image snapshots are in `artifacts/firmware-candidates/element-mf-v0.10/`; the installation record is `artifacts/hardware-test-element-mf-v0.10/device.json`.
- Mf reports use encoder 2 (byte 5) for Jog, button 15 (byte 1, bit 7) for Play, and button 14 (byte 1, bit 6) for Stop. Shipped-map assignments are saved in `artifacts/research/firmware/flame-default-mf-transport.json`; 37 modes were checked. Native Play does not toggle Stop, and no playback feedback appeared in the captured HighlightControl messages. The user chose explicit tap-to-Play and hold-to-Stop gestures instead of alternating presses. A long hold never sends Play first; a tap sends a Play down/up pair on release. No playback-state tracking is required.
- Hardware evidence: v0.4 produced 467 native Transport events with shuttle zero and fixed the packet-memory overlap (493 clean input reports). On v0.5 the operator confirmed all controls; capture shows 109 Transport events, five native Play down/up pairs, and two native Stop down/up pairs. See `artifacts/hardware-test-element-mf-v0.5/verification.json`. Captures were stopped cleanly with no kernel drops. v0.6 only changes the hold threshold and revision number.
- Read-only Hub analysis confirms the Mf encoder offset. It normalizes the ring at 1180 counts/revolution and 16 jog steps/revolution with default sensitivity 1. Revision 4 sends 74 raw counts per detent (approximately one jog step), versus 1 previously. It also allocates 16 bytes for the 15-byte IN report to avoid overlapping the OUT packet-memory buffer. Evidence: `artifacts/research/firmware/mf-jog-normalization.json`.
- `make setup`, `make test`, and `make firmware` were available. Checks at that checkpoint: 20 Python tests and two native C test programs passed. ARM builds and DFU suffixes passed. Dependencies are pinned in `build-lock.json` and cached locally.
- Real Element Kb was connected for protocol capture, then unplugged for the emulator test. Before the November 18 comparison, the real Mf had not yet been captured. Kb captures and decoded summaries are in `artifacts/element-kb-20261116/`; the later Mf evidence is described above.
- The application at that stage was Autodesk Flame 2027.2. Earlier verification used Flare 2027.2; its captured ApplicationDefinition named `Autodesk Flare`, system maps `/opt/Autodesk/presets/2027.2/tangentPanel`, and an empty user-map directory. At that stage, Hub's panel-list had not been edited and expected one of each panel type; the later observed serial assignments are recorded above.
- Tangent Hub was restarted once; Flare did not reconnect automatically (old socket CLOSE-WAIT). User saved/restarted Flare and reconnection was captured. Avoid further Hub restarts unless planned with the operator. Mapper was opened but no mapping was changed; its window was gone when cleanup was attempted.
- User requested only steady white bottom lighting at low intensity. The pinned QMK `keyboards/binepad/knobx1/keyboard.json` specifies two WS2812 LEDs on PC13. `underglow.c` writes `(2,2,2)` once at startup (under 1% PWM level), with no animation. A 72 MHz DWT counter schedules 800 kHz data pulses before USB initialization; unsupported cycle counting exits safely. The compiled pulse loop was inspected. The user found level 12 too bright, requested a small increase from 2, then reported level 3 too bright. v0.10 restores level 2. No further confirmation request is pending. This lighting does not represent playback state.

## Intended behavior

Use the KnobX1 with Flame's native Tangent integration:

- Use the four accepted Jog/fast-scrubbing pages and reversed orientations described in the verified firmware section; this supersedes the earlier Jog-only restriction.
- Tap and release the large knob for Play; hold for 300 ms for Stop. This explicitly supersedes the earlier request for alternating large-knob presses.
- Hold-and-turn acceleration is not implemented; the user accepted omitting that gesture.
- The implementation runs on the knob itself, with no external USB adapter or workstation bridge.
- Use Flame's shipped default profile. A custom transport profile is explicitly rejected.
- Both bottom RGB LEDs should remain steady white at low intensity.

The earlier iWit investigation led to purchasing this KnobX1. Other hardware recommendations are no longer the active task. The working directory is `/vol/engineering/dev/tangential`; the earlier Flame repository is separate. `AGENTS.md` contains the current contributor guide.

## Other hardware research — 2026-11-18

The user asked about the Hagibis knob/hub. Research covered the UC-1102A/UC-1102AG family: [Hagibis documents volume/mute and shortcut functions](https://www.hagibis.com.cn/products/knob-usb-c-hub-342), while the community [Hubibis project](https://github.com/Irukandji-1337/hubibis) remaps inputs using Windows/AutoHotkey software. No confirmed custom-firmware programming path for the knob controller was found. Onboard Tangent emulation remains unverified on that hardware and would require identifying its controller, programming access, and recovery method. No Hagibis device was inspected or modified.

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

Firmware candidate manifests record build-time facts and retain the initial "not yet flashed" status. Do not treat that field as the current installation state: use the corresponding hardware-test directory and the guarded emulator `status` command. Preserve these historical manifests rather than rewriting the evidence.

The Python CLI uses only the standard library. Its command allowlist permits configuration getters; it has no flashing, reset, unlock, or remapping operation. Examples from the repository root:

```bash
python3 scripts/inspect_knobx1.py --help
python3 scripts/inspect_knobx1.py inspect --output artifacts/baseline-NEW.json
python3 scripts/inspect_knobx1.py monitor --seconds 120 --output artifacts/inputs-NEW.jsonl
```

Use new output filenames. Hardware access required execution outside the sandbox because `/dev/hidraw*` nodes were hidden inside it. On the host, the HID nodes were accessible to the session user; evdev nodes were not. No permission rules were changed. Monitoring is passive and leaves the knob's normal volume/mute behavior active. Arrange operator movements while capture is running.

## Firmware development evidence

The published [QMK KnobX1 definition](https://github.com/qmk/qmk_firmware/tree/master/keyboards/binepad/knobx1) identifies an STM32F103 and STM32duino bootloader. Its documented bootloader gesture is holding the small layer button while connecting USB. The original VID/PID matches the published definition. The existing bootloader identity, application address, cold-start recovery, and repeated firmware updates have now been verified on this unit. Restoring the original firmware remains untested.

The [Vial QMK KnobX1 directory](https://github.com/vial-kb/vial-qmk/tree/vial/keyboards/binepad/knobx1) also contains board code. The checked branch exposed a default keymap, but a matching stock Vial build and recovery image were not located. Do not assume the upstream default reproduces the installed firmware exactly. `dfu-util`, `arm-none-eabi-gcc`, `qmk`, and `openocd` were absent from PATH at inspection.

## Tangent and Flame protocol findings

Onboard emulation reproduces a physical Tangent panel's USB behavior so Tangent Hub recognizes it. Kb USB descriptors and inputs were captured from the real panel; Mf identity and encoder layout were checked against the installed Hub. Native Jog, Play, and Stop were verified through captures and operator testing on v0.5 in Flare and v0.10 in Flame. The 300 ms boundary is also tested offline.

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

The earlier workstation-bridge discussion is historical. No workstation bridge is implemented or required. The current requirement is onboard USB emulation through the standard Tangent Hub and Flame's shipped profile; do not substitute another Flame API, keyboard shortcut layer, or custom map.

## Follow-up verification

1. Preserve the accepted v0.11 four-page controls, 300 ms Stop hold, and RGB `(2,2,2)` underglow. Page operation has operator confirmation in Flare; no input request or capture monitor is pending/running. Continuous-speed Shuttle is explicitly deferred.
2. Portability beyond this workstation's tested Flame/Flare builds, simultaneous operation with a real Mf, and restoring the original firmware remain untested. The real Mf descriptor and control-report comparisons are now complete; do not claim universal compatibility.
3. Keep underglow steady dim white; do not add animations or playback-state indication unless requested.

The original baseline above is historical evidence. The active firmware/build/test state is recorded at the top of this handoff and in the artifact directories. USB/Hub recognition, native Jog, tap-to-Play and hold-to-Stop are verified with the shipped default Mf profile in this workstation's Flame and Flare setup.

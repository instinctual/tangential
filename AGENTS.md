# Repository Guidelines

## Project Structure & Module Organization

This project develops onboard Tangent emulation for the Binepad KnobX1. The private repository is [instinctual/tangential](https://github.com/instinctual/tangential), default branch `main`. Read `HANDOFF.md` for firmware state, evidence, recovery, and outstanding verification.

Use Element Mf USB emulation through Tangent Hub and Flame's shipped profile. Do not substitute custom maps, other Flame APIs, keyboard shortcuts, or workstation bridges. Preserve these controls: rotate to Jog, tap/release to Play, hold 300 ms to Stop, and steady white bottom LEDs at RGB `(2, 2, 2)`. Levels 3 and 12 were too bright.

- `src/firmware/`: STM32 application, gestures, lighting, and linker script.
- `src/tangential/`: report and capture parsers.
- `scripts/`: inspection, build, capture, and separate bootloader tools.
- `tests/`: offline Python and native C tests.
- `artifacts/`: captures, firmware snapshots, and original readbacks.
- `.cache/`, `.venv/`, `build/`: ignored dependencies and generated output.

## Build, Test, and Development Commands

Run from the repository root. Firmware setup requires Linux x86_64, Python 3.12+, a C compiler, Make, pkg-config, and libusb headers; see `README.md`.

- `make setup`: fetch pinned dependencies and build tools.
- `make test`: run offline Python and C tests.
- `make firmware`: compile and validate without flashing.
- `python3 scripts/inspect_knobx1.py --help`: show the original VIA/Vial inspection CLI.
- `python3 scripts/emulator_control.py status`: read emulator diagnostics.

Use fresh artifact filenames. Hardware tests are opt-in and require device access and identity checks.

## Coding Style & Naming Conventions

Use four spaces, `snake_case` functions/files, `PascalCase` classes, and `UPPER_SNAKE_CASE` constants. Prefer standard-library Python and focused functions. No formatter or linter is configured. Preserve byte order, bounded reads, and device checks.

## Testing Guidelines

Use `unittest` in `tests/test_*.py` and native C tests. Cover malformed replies, timeouts, command restrictions, decoding, and gesture boundaries. Run `make test`; no coverage threshold is set.

## Commit & Pull Request Guidelines

Use imperative subjects, following `Add KnobX1 Tangent transport firmware`. Keep changes focused. PRs should explain behavior, validation, linked issues, hardware requirements, and persistent device changes.

## Hardware & Evidence Handling

Keep inspection to getters and passive reads; bootloader entry and flashing are separate operations. Preserve evidence byte for byte (`.gitattributes`); configuration snapshots are not firmware backups. Never overwrite the resident bootloader. Build manifests describe build-time state; `artifacts/hardware-test-*/` records installation results. Verify Hub/Flame behavior before claiming compatibility. Exclude credentials and unrelated workstation data.

# Repository Guidelines

## Project Structure & Module Organization

This repository develops experimental onboard Tangent panel emulation for the Binepad KnobX1, with Linux inspection tools and preserved hardware evidence.

The device must work with Flame's shipped default Tangent profile. Do not introduce custom maps, a keyboard-shortcut layer, or a workstation bridge to satisfy the transport controls. Target the Element Mf's native jog ring and transport buttons.

- `scripts/inspect_knobx1.py`: standard-library Python CLI for USB/HID discovery, VIA/Vial configuration reads, and passive input monitoring.
- `artifacts/`: dated JSON configuration snapshots, JSONL input captures, and inspection results.
- `src/firmware/`: STM32 firmware and the bootloader-preserving linker script.
- `src/tangential/`: report and USB-capture parsing.
- `tests/`: offline Python and native C tests.
- `.cache/`, `.venv/`, and `build/`: ignored local dependencies and generated output.

## Build, Test, and Development Commands

Run commands from the repository root. Inspection uses standard-library Python; firmware setup requires Python 3.12+, Linux x86_64, a native C compiler, Make, pkg-config, and libusb development headers. See `README.md`.

- `make setup`: fetch checksum-pinned dependencies and build local tools.
- `make test`: run offline Python and C protocol tests.
- `make firmware`: compile and validate the experimental DFU image without flashing.

- `python3 scripts/inspect_knobx1.py --help`: show CLI options without accessing hardware.
- `python3 scripts/inspect_knobx1.py inspect --output artifacts/baseline-YYYYMMDD.json`: read device descriptors, definitions, and mappings.
- `python3 scripts/inspect_knobx1.py monitor --seconds 60 --output artifacts/inputs-YYYYMMDD.jsonl`: capture input reports while an operator rotates and presses the knob.
- `python3 -c "import ast,pathlib; ast.parse(pathlib.Path('scripts/inspect_knobx1.py').read_text())"`: check Python syntax without connecting to hardware.

Hardware commands require Linux, one connected KnobX1, and access to its `/dev/hidraw*` nodes. Choose fresh output filenames; existing files are rejected.

## Coding Style & Naming Conventions

Use four-space indentation, `snake_case` functions and filenames, `PascalCase` classes, and `UPPER_SNAKE_CASE` constants. Prefer standard-library modules and small, focused functions. No formatter or linter is configured. Preserve explicit protocol byte order, bounded reads, and device identity checks.

## Testing Guidelines

Use standard-library `unittest` under `tests/test_*.py` and native C tests for firmware protocol behavior. Run `make test`; no numeric coverage threshold is set. Cover malformed replies, timeouts, decoding, and command restrictions. Keep hardware tests opt-in; record device identity and distinguish observed input from configured mappings.

## Commit & Pull Request Guidelines

No commit convention could be established from available Git history. Use short imperative subjects, such as `Add HID response validation`. Keep changes focused. PRs should explain behavior, list validation commands, link relevant issues, and identify any hardware requirements or persistent device changes.

## Hardware & Evidence Handling

Keep inspection restricted to documented getters and passive reads. Bootloader entry, flashing, and remapping are separate operations. Preserve baselines and original firmware readbacks; configuration snapshots alone are not firmware backups. Never include the resident bootloader region in an application image. Verify Tangent Hub and Flame behavior before claiming compatibility. Exclude credentials and unrelated workstation data from committed artifacts.

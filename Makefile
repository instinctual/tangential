PYTHON ?= python3
BUILD_PYTHON ?= python3.12
CC ?= cc
LIBOPENCM3 := .cache/vendor/libopencm3-a3977e14519e4cff8d1feb96445655465797908b

.PHONY: setup test firmware inspect-help

setup:
	$(BUILD_PYTHON) scripts/setup_build.py

test:
	$(PYTHON) -m unittest discover -s tests -v
	mkdir -p build
	$(CC) -std=c11 -Wall -Wextra -Werror -Isrc/firmware tests/firmware_report_test.c -o build/firmware-report-test
	./build/firmware-report-test
	$(CC) -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DSTM32F1 -I$(LIBOPENCM3)/include -Isrc/firmware tests/firmware_usb_test.c -Wl,--gc-sections -o build/firmware-usb-test
	./build/firmware-usb-test

firmware:
	$(PYTHON) scripts/build_firmware.py

inspect-help:
	$(PYTHON) scripts/inspect_knobx1.py --help

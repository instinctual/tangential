#ifndef TANGENTIAL_TRANSPORT_PAGES_H
#define TANGENTIAL_TRANSPORT_PAGES_H

#include <stdbool.h>
#include <stdint.h>
#include "element_report.h"

#define TRANSPORT_PAGE_COUNT 4
#define PAGE_BUTTON_BOOT_HOLD_MS 2000

/* Pages 1/2 use the Mf ring; pages 3/4 use the horizontal trackball,
 * labelled Shuttle in Flame's shipped map. Even-numbered pages reverse
 * the physical encoder direction. Page indices here are zero-based. */
static inline uint8_t transport_page_encoder(uint8_t page) {
    return page < 2 ? 2 : 0;
}

static inline int32_t transport_page_counts(uint8_t page, int8_t physical_step) {
    int32_t counts = (int32_t)physical_step * ELEMENT_MF_JOG_COUNTS_PER_STEP;
    return (page & 1) ? -counts : counts;
}

typedef enum {
    PAGE_BUTTON_NONE,
    PAGE_BUTTON_ADVANCE,
    PAGE_BUTTON_BOOTLOADER,
} page_button_action;

typedef struct {
    bool down;
    bool boot_sent;
    uint32_t down_since;
} page_button_state;

/* Input is debounced. Short release changes page; a long hold retains
 * the separate bootloader gesture and never also advances the page. */
static inline page_button_action page_button_update(page_button_state *state,
    bool down, uint32_t now) {
    if (down && !state->down) {
        state->down = true;
        state->boot_sent = false;
        state->down_since = now;
        return PAGE_BUTTON_NONE;
    }
    if (!state->down) return PAGE_BUTTON_NONE;
    bool held = (uint32_t)(now - state->down_since) >= PAGE_BUTTON_BOOT_HOLD_MS;
    if (down) {
        if (held && !state->boot_sent) {
            state->boot_sent = true;
            return PAGE_BUTTON_BOOTLOADER;
        }
        return PAGE_BUTTON_NONE;
    }
    state->down = false;
    if (state->boot_sent) {
        state->boot_sent = false;
        return PAGE_BUTTON_NONE;
    }
    return held ? PAGE_BUTTON_BOOTLOADER : PAGE_BUTTON_ADVANCE;
}

#endif

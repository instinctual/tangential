#ifndef TANGENTIAL_ELEMENT_REPORT_H
#define TANGENTIAL_ELEMENT_REPORT_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Hub 1.9.8 normalizes the Mf ring to 1180 counts/revolution and sends
 * 16 jog steps/revolution at Flame's default sensitivity: 73.75 counts
 * per step. Round up so a single physical detent produces a jog step.
 * This remains a native ring delta; no application action is substituted. */
#define ELEMENT_MF_JOG_COUNTS_PER_STEP 74

/* Element Kb wire layout observed in artifacts/element-kb-20261116. */
static inline void element_first_knob_report(uint8_t report[15], bool pressed, int8_t delta) {
    memset(report, 0, 15);
    report[0] = pressed ? 1 : 0;
    report[3] = (uint8_t)delta;
}

/* Flame's shipped Element-Mf map assigns encoder 2 to Jog and button 15
 * to Play. The shared Element transport carries three button bytes followed
 * by signed encoder bytes. Playback toggle behavior is verified separately. */
static inline void element_mf_transport_report(uint8_t report[15], bool pressed, int8_t delta) {
    memset(report, 0, 15);
    report[1] = pressed ? 0x80 : 0;
    report[5] = (uint8_t)delta;
}

/* A and B are the same pin ordering used in the published KnobX1 definition. */
static inline int8_t encoder_transition(uint8_t previous, uint8_t current) {
    uint8_t changed = previous ^ current;
    if (changed == 0 || changed == 3) {
        return 0;
    }
    return (((previous >> 1) ^ current) & 1) ? -1 : 1;
}

typedef struct {
    uint8_t phase;
    int8_t fraction;
} encoder_state;

static inline int8_t encoder_read_step(encoder_state *state, uint8_t current) {
    state->fraction += encoder_transition(state->phase, current);
    state->phase = current;
    /* QMK labels negative transitions clockwise for this pin ordering.
     * The captured Element Kb uses positive bytes for clockwise motion. */
    if (state->fraction <= -2) {
        state->fraction += 2;
        return 1;
    }
    if (state->fraction >= 2) {
        state->fraction -= 2;
        return -1;
    }
    return 0;
}

#endif


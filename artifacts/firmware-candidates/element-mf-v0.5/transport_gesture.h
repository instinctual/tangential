#ifndef TANGENTIAL_TRANSPORT_GESTURE_H
#define TANGENTIAL_TRANSPORT_GESTURE_H

#include <stdbool.h>
#include <stdint.h>

#define TRANSPORT_STOP_HOLD_MS 600
#define ELEMENT_MF_PLAY_BUTTON 0x80
#define ELEMENT_MF_STOP_BUTTON 0x40

typedef struct {
    bool down;
    bool stop_sent;
    uint32_t down_since;
} transport_gesture;

/* Input is already debounced. A tap generates a Play down/up pair only
 * on release, so holding Stop never starts playback first. A held Stop
 * stays down until physical release, matching a real panel button. */
static inline unsigned transport_gesture_update(transport_gesture *state,
    bool down, uint32_t now, uint8_t events[2]) {
    if (down && !state->down) {
        state->down = true;
        state->stop_sent = false;
        state->down_since = now;
        return 0;
    }
    if (!state->down) return 0;
    bool held = (uint32_t)(now - state->down_since) >= TRANSPORT_STOP_HOLD_MS;
    if (down) {
        if (held && !state->stop_sent) {
            state->stop_sent = true;
            events[0] = ELEMENT_MF_STOP_BUTTON;
            return 1;
        }
        return 0;
    }
    state->down = false;
    if (state->stop_sent) {
        state->stop_sent = false;
        events[0] = 0;
        return 1;
    }
    /* Also handle release after the threshold if the loop was delayed. */
    events[0] = held ? ELEMENT_MF_STOP_BUTTON : ELEMENT_MF_PLAY_BUTTON;
    events[1] = 0;
    return 2;
}

#endif

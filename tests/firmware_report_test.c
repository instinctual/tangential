#include <assert.h>
#include <stdio.h>
#include "element_report.h"
#include "transport_gesture.h"
#include "transport_pages.h"

static void test_transport_pages(void) {
    for (uint8_t page = 0; page < TRANSPORT_PAGE_COUNT; page++) {
        uint8_t encoder = transport_page_encoder(page);
        assert(encoder == (page < 2 ? 2 : 0));
        for (int direction = -1; direction <= 1; direction += 2) {
            int32_t counts = transport_page_counts(page, direction);
            assert(counts == ((page & 1) ? -direction : direction) * 74);
            for (unsigned held = 0; held < 3; held++) {
                uint8_t button = held == 1 ? ELEMENT_MF_PLAY_BUTTON : held == 2 ? ELEMENT_MF_STOP_BUTTON : 0;
                uint8_t report[15];
                element_mf_transport_report(report, button, counts, encoder);
                assert(report[1] == button && (int8_t)report[3 + encoder] == counts);
                for (unsigned i = 0; i < 15; i++) {
                    if (i != 1 && i != 3u + encoder) assert(report[i] == 0);
                }
            }
        }
    }

    page_button_state state = {0};
    assert(page_button_update(&state, false, 0) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, true, 100) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, true, 200) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, false, 250) == PAGE_BUTTON_ADVANCE);
    assert(page_button_update(&state, false, 300) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, true, 1000) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, true, 2999) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, true, 3000) == PAGE_BUTTON_BOOTLOADER);
    assert(page_button_update(&state, true, 4000) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, false, 4001) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, true, 5000) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, false, 7000) == PAGE_BUTTON_BOOTLOADER);
    assert(page_button_update(&state, false, 7100) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, true, UINT32_MAX - 100) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, true, 1898) == PAGE_BUTTON_NONE);
    assert(page_button_update(&state, true, 1899) == PAGE_BUTTON_BOOTLOADER);
    assert(page_button_update(&state, false, 1900) == PAGE_BUTTON_NONE);
}

static void test_transport_gestures(void) {
    transport_gesture state = {0};
    uint8_t events[2];
    assert(transport_gesture_update(&state, false, 0, events) == 0);
    assert(transport_gesture_update(&state, true, 100, events) == 0);
    assert(transport_gesture_update(&state, true, 200, events) == 0);
    assert(transport_gesture_update(&state, false, 399, events) == 2);
    assert(events[0] == ELEMENT_MF_PLAY_BUTTON && events[1] == 0);
    assert(transport_gesture_update(&state, false, 400, events) == 0);

    /* Hold emits Stop once at the threshold, never Play, then release. */
    assert(transport_gesture_update(&state, true, 1000, events) == 0);
    assert(transport_gesture_update(&state, true, 1299, events) == 0);
    assert(transport_gesture_update(&state, true, 1300, events) == 1);
    assert(events[0] == ELEMENT_MF_STOP_BUTTON);
    assert(transport_gesture_update(&state, true, 3000, events) == 0);
    assert(transport_gesture_update(&state, false, 3001, events) == 1 && events[0] == 0);

    /* A delayed loop must not misclassify a long press as a Play tap. */
    assert(transport_gesture_update(&state, true, 4000, events) == 0);
    assert(transport_gesture_update(&state, false, 4300, events) == 2);
    assert(events[0] == ELEMENT_MF_STOP_BUTTON && events[1] == 0);
    assert(transport_gesture_update(&state, true, 5000, events) == 0);
    assert(transport_gesture_update(&state, false, 5100, events) == 2);
    assert(events[0] == ELEMENT_MF_PLAY_BUTTON && events[1] == 0);

    /* Millisecond counter rollover must preserve hold timing. */
    assert(transport_gesture_update(&state, true, UINT32_MAX - 100, events) == 0);
    assert(transport_gesture_update(&state, true, 198, events) == 0);
    assert(transport_gesture_update(&state, true, 199, events) == 1);
    assert(events[0] == ELEMENT_MF_STOP_BUTTON);
    assert(transport_gesture_update(&state, false, 200, events) == 1 && events[0] == 0);
}

int main(void) {
    uint8_t result[15];
    const uint8_t clockwise[15] = {0,0,0,1};
    const uint8_t counterclockwise[15] = {0,0,0,0xff};
    const uint8_t push[15] = {1};
    element_first_knob_report(result, false, 1);
    assert(memcmp(result, clockwise, 15) == 0);
    element_first_knob_report(result, false, -1);
    assert(memcmp(result, counterclockwise, 15) == 0);
    element_first_knob_report(result, true, 0);
    assert(memcmp(result, push, 15) == 0);
    element_first_knob_report(result, false, 0);
    assert(memcmp(result, (uint8_t[15]){0}, 15) == 0);
    element_mf_transport_report(result, ELEMENT_MF_PLAY_BUTTON, 1, 2);
    assert(memcmp(result, (uint8_t[15]){0,0x80,0,0,0,1}, 15) == 0);
    element_mf_transport_report(result, ELEMENT_MF_STOP_BUTTON, -1, 2);
    assert(memcmp(result, (uint8_t[15]){0,0x40,0,0,0,0xff}, 15) == 0);
    element_mf_transport_report(result, false, -1, 2);
    assert(memcmp(result, (uint8_t[15]){0,0,0,0,0,0xff}, 15) == 0);
    element_mf_transport_report(result, false, 0, 2);
    assert(memcmp(result, (uint8_t[15]){0}, 15) == 0);
    /* Hub's default Mf jog normalization must produce one whole step in
     * either direction, rather than accumulating invisible partial steps. */
    assert(ELEMENT_MF_JOG_COUNTS_PER_STEP <= 127);
    for (int direction = -1; direction <= 1; direction += 2) {
        element_mf_transport_report(result, false, direction * ELEMENT_MF_JOG_COUNTS_PER_STEP, 2);
        float jog = (int8_t)result[5] * 16.0f / 1180.0f;
        assert((int)jog == direction);
    }
    assert(encoder_transition(0, 3) == 0);
    assert(encoder_transition(2, 2) == 0);
    int one_cycle = encoder_transition(0, 2) + encoder_transition(2, 3)
                  + encoder_transition(3, 1) + encoder_transition(1, 0);
    assert(one_cycle == 4);
    assert(encoder_transition(0, 1) == -encoder_transition(1, 0));
    encoder_state state = {.phase = 3, .fraction = 0};
    int clockwise_steps = encoder_read_step(&state, 2);
    clockwise_steps += encoder_read_step(&state, 0);
    clockwise_steps += encoder_read_step(&state, 1);
    clockwise_steps += encoder_read_step(&state, 3);
    assert(clockwise_steps == 2);
    /* A transition and its bounce back must not produce a jog event. */
    assert(encoder_read_step(&state, 2) == 0);
    assert(encoder_read_step(&state, 3) == 0);
    test_transport_gestures();
    test_transport_pages();
    puts("Firmware report, quadrature, transport gesture and page tests passed");
    return 0;
}


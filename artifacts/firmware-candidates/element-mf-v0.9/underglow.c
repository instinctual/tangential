/* KnobX1: two WS2812 LEDs on PC13, from its pinned QMK board definition.
 * Set a static color once at startup, before USB polling begins. */
#include <stdint.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/scs.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include "underglow.h"

void underglow_init_dim_white(void) {
    enum { LED_COUNT = 2, WHITE_LEVEL = 3, BIT_CYCLES = 90 };
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_clear(GPIOC, GPIO13);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

    SCS_DEMCR |= SCS_DEMCR_TRCENA;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;
    uint32_t before = DWT_CYCCNT;
    for (unsigned i = 0; i < 32; i++) __asm__ volatile("nop");
    /* Unsupported cycle counting must not prevent USB/transport startup. */
    if (DWT_CYCCNT == before) return;

    /* At 72 MHz: 90 cycles/bit = 800 kHz; high is about 0.36 or 0.72 us.
     * Absolute edge deadlines avoid extra gaps at byte/LED boundaries.
     * Equal RGB channels mean every GRB byte is WHITE_LEVEL (about 1.2%). */
    uint32_t previous_mask = cm_mask_interrupts(1);
    uint32_t rise_at = DWT_CYCCNT + 72000; /* Initial 1 ms reset/latch gap. */
    for (unsigned bit = 0; bit < LED_COUNT * 24; bit++) {
        uint32_t fall_at = rise_at + ((WHITE_LEVEL & (0x80u >> (bit & 7))) ? 52 : 26);
        while ((int32_t)(DWT_CYCCNT - rise_at) < 0) {}
        GPIO_BSRR(GPIOC) = GPIO13;
        while ((int32_t)(DWT_CYCCNT - fall_at) < 0) {}
        GPIO_BRR(GPIOC) = GPIO13;
        rise_at += BIT_CYCLES;
    }
    cm_mask_interrupts(previous_mask);
    /* Data remains low. The LEDs latch and retain this color without any
     * repeated updates in the USB/input loop. */
}

/* Tangential: experimental KnobX1 Element-Mf USB firmware.
 * GPIO assignments: published QMK binepad/knobx1 definition.
 * Shared Element USB layout: captured from the owner's Element Kb.
 * Mf identity/report length: installed Tangent Hub; transport assignments:
 * Flame's shipped element-mf-map.xml. No custom map is required.
 * No flash-writing code is linked; the existing bootloader stays in place.
 */
#include <stdbool.h>
#include <stdint.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/f1/bkp.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/usbstd.h>
#include <libopencm3/usb/hid.h>
#include "element_report.h"
#include "transport_gesture.h"
#include "underglow.h"

static volatile uint32_t milliseconds;
static bool configured;
static bool pressed;
static uint8_t idle_rate;
static uint8_t last_report[15];
static uint8_t control_buffer[128];
static uint8_t button_queue[16];
static unsigned queue_read, queue_write;
static uint8_t sent_buttons;
static transport_gesture gesture;
static int32_t pending_counts;
static bool boot_requested;
static uint8_t diagnostic_report[12];

static const uint8_t report_descriptor[] = {
    0x05,0x01,0x09,0x08,0xa1,0x01,0x05,0xf0,0x09,0x01,0x75,0x08,
    0x95,0x0f,0x81,0x02,0x05,0xf0,0x09,0x02,0x75,0x08,0x95,0x40,
    0x91,0x02,0xc0
};

static const struct usb_device_descriptor device_descriptor = {
    .bLength = USB_DT_DEVICE_SIZE, .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200, .bDeviceClass = 0, .bDeviceSubClass = 0,
    .bDeviceProtocol = 0, .bMaxPacketSize0 = 8,
    .idVendor = 0x04d8, .idProduct = 0xfa1a, .bcdDevice = 0x0120,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3,
    .bNumConfigurations = 2,
};

static const uint8_t hid_descriptor[] = {
    9, 0x21, 0x11, 0x01, 0, 1, 0x22, sizeof(report_descriptor), 0
};

static const struct usb_endpoint_descriptor endpoints[] = {
    {.bLength = USB_DT_ENDPOINT_SIZE, .bDescriptorType = USB_DT_ENDPOINT,
     .bEndpointAddress = 0x81, .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
     .wMaxPacketSize = 15, .bInterval = 20},
    {.bLength = USB_DT_ENDPOINT_SIZE, .bDescriptorType = USB_DT_ENDPOINT,
     .bEndpointAddress = 0x01, .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
     .wMaxPacketSize = 64, .bInterval = 16},
};

static const struct usb_interface_descriptor interface_descriptor = {
    .bLength = USB_DT_INTERFACE_SIZE, .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0, .bAlternateSetting = 0, .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_HID, .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0, .iInterface = 0, .endpoint = endpoints,
    .extra = hid_descriptor, .extralen = sizeof(hid_descriptor),
};

static const struct usb_interface interfaces[] = {
    {.num_altsetting = 1, .altsetting = &interface_descriptor},
};

static const struct usb_config_descriptor configurations[] = {
    {.bLength = USB_DT_CONFIGURATION_SIZE, .bDescriptorType = USB_DT_CONFIGURATION,
     .wTotalLength = 0, .bNumInterfaces = 1, .bConfigurationValue = 1,
     .iConfiguration = 0, .bmAttributes = 0x80, .bMaxPower = 250,
     .interface = interfaces},
    {.bLength = USB_DT_CONFIGURATION_SIZE, .bDescriptorType = USB_DT_CONFIGURATION,
     .wTotalLength = 0, .bNumInterfaces = 1, .bConfigurationValue = 2,
     .iConfiguration = 0, .bmAttributes = 0x80, .bMaxPower = 50,
     .interface = interfaces},
};

/* Keep a separate numeric serial so the real panel can remain connected. */
static const char *const strings[] = {
    "Tangential", "KnobX1 Element-Mf transport", "99000101"
};

void sys_tick_handler(void) {
    milliseconds++;
}

static void delay_ms(uint32_t duration) {
    uint32_t start = milliseconds;
    while ((uint32_t)(milliseconds - start) < duration) {
        __asm__("nop");
    }
}

static void enter_bootloader(void) {
    rcc_periph_clock_enable(RCC_PWR);
    rcc_periph_clock_enable(RCC_BKP);
    pwr_disable_backup_domain_write_protect();
    BKP_DR10 = 0x424c; /* STM32duino's persistent bootloader flag. */
    (void)BKP_DR10; /* Complete the APB write before requesting a core reset. */
#if defined(__arm__)
    __asm__ volatile("dsb" ::: "memory");
#endif
    scb_reset_system();
}

static void boot_control_complete(usbd_device *device, struct usb_setup_data *request) {
    (void)device;
    (void)request;
    boot_requested = true;
}

static enum usbd_request_return_codes development_control(
    usbd_device *device, struct usb_setup_data *request,
    uint8_t **buffer, uint16_t *length, usbd_control_complete_callback *complete) {
    (void)device;
    if (request->bRequest != 0x54 || request->wIndex != 0x5831) {
        return USBD_REQ_NEXT_CALLBACK;
    }
    if (request->bmRequestType == 0x40 && request->wValue == 0x424c && request->wLength == 0) {
        *length = 0;
        *complete = boot_control_complete;
        return USBD_REQ_HANDLED;
    }
    if (request->bmRequestType == 0xc0 && request->wValue == 1) {
        uint16_t a = gpio_get(GPIOA, 0xffff), b = gpio_get(GPIOB, 0xffff);
        uint32_t now = milliseconds;
        diagnostic_report[0] = a;
        diagnostic_report[1] = a >> 8;
        diagnostic_report[2] = b;
        diagnostic_report[3] = b >> 8;
        for (unsigned i = 0; i < 4; i++) diagnostic_report[4 + i] = now >> (8 * i);
        diagnostic_report[8] = configured;
        diagnostic_report[9] = pressed;
        diagnostic_report[10] = 10; /* Development protocol/build revision. */
        diagnostic_report[11] = 0;
        *buffer = diagnostic_report;
        if (*length > sizeof(diagnostic_report)) *length = sizeof(diagnostic_report);
        return USBD_REQ_HANDLED;
    }
    return USBD_REQ_NEXT_CALLBACK;
}

static enum usbd_request_return_codes hid_control(
    usbd_device *device, struct usb_setup_data *request,
    uint8_t **buffer, uint16_t *length, usbd_control_complete_callback *complete) {
    (void)device;
    (void)complete;
    if (request->wIndex != 0) {
        return USBD_REQ_NEXT_CALLBACK;
    }
    if (request->bmRequestType == 0x81 && request->bRequest == USB_REQ_GET_DESCRIPTOR &&
        request->wValue == 0x2200) {
        *buffer = (uint8_t *)report_descriptor;
        if (*length > sizeof(report_descriptor)) *length = sizeof(report_descriptor);
        return USBD_REQ_HANDLED;
    }
    if (request->bmRequestType == 0x21 && request->bRequest == 0x0a) {
        idle_rate = request->wValue >> 8;
        return USBD_REQ_HANDLED;
    }
    if (request->bmRequestType == 0xa1 && request->bRequest == 0x02) {
        *buffer = &idle_rate;
        *length = 1;
        return USBD_REQ_HANDLED;
    }
    if (request->bmRequestType == 0xa1 && request->bRequest == 0x01 &&
        (request->wValue >> 8) == 1) {
        *buffer = last_report;
        if (*length > sizeof(last_report)) *length = sizeof(last_report);
        return USBD_REQ_HANDLED;
    }
    return USBD_REQ_NEXT_CALLBACK;
}

static void output_received(usbd_device *device, uint8_t endpoint) {
    uint8_t buffer[64];
    /* Drain display/heartbeat commands; this knob has no panel display. */
    usbd_ep_read_packet(device, endpoint, buffer, sizeof(buffer));
}

static void reset_received(void) {
    configured = false;
    gpio_clear(GPIOA, GPIO6);
}

static void queue_button_events(const uint8_t *events, unsigned count) {
    if (!count) return;
    unsigned available = (queue_read + 15 - queue_write) % 16;
    /* Enqueue taps atomically and reserve room to release a held button. */
    unsigned needed = events[count - 1] ? count + 1 : count;
    if (available < needed) {
        if (count == 1 && events[0] == 0) {
            /* Never leave a native button held if the bounded queue fills. */
            queue_read = queue_write = 0;
        } else {
            return;
        }
    }
    for (unsigned i = 0; i < count; i++) {
        button_queue[queue_write] = events[i];
        queue_write = (queue_write + 1) % 16;
    }
}

static void configuration_set(usbd_device *device, uint16_t value) {
    configured = value == 1 || value == 2;
    queue_read = 0;
    queue_write = 1;
    button_queue[0] = 0;
    sent_buttons = 0;
    memset(&gesture, 0, sizeof(gesture));
    pending_counts = 0;
    /* libopencm3 clears control callbacks on SET_CONFIGURATION. */
    usbd_register_control_callback(device, 0x01, 0x1f, hid_control);
    usbd_register_control_callback(device, 0x40, 0x7f, development_control);
    if (configured) {
        /* STM32 USB packet memory uses 16-bit words. libopencm3 advances
         * pm_top by this size without rounding: 15 would overlap the last
         * IN byte with the OUT buffer. Advertise and transmit 15, reserve 16. */
        usbd_ep_setup(device, 0x81, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);
        usbd_ep_setup(device, 0x01, USB_ENDPOINT_ATTR_INTERRUPT, 64, output_received);
        gpio_set(GPIOA, GPIO6);
    } else {
        gpio_clear(GPIOA, GPIO6);
    }
}

int main(void) {
    SCB_VTOR = 0x08002000;
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_reload(72000 - 1);
    systick_interrupt_enable();
    systick_counter_enable();

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_AFIO);
    gpio_primary_remap(AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON, 0); /* Free PA15. */
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO8);
    gpio_clear(GPIOB, GPIO8); /* Common switch row. */
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO7 | GPIO15);
    gpio_set(GPIOA, GPIO7 | GPIO15);
    gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO12 | GPIO13);
    gpio_set(GPIOB, GPIO12 | GPIO13);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,
                  GPIO3 | GPIO4 | GPIO5 | GPIO6);
    gpio_clear(GPIOA, GPIO3 | GPIO4 | GPIO5 | GPIO6);
    delay_ms(10);
    if (!gpio_get(GPIOA, GPIO7)) enter_bootloader();
    underglow_init_dim_white();

    /* Force a clean enumeration when the bootloader jumps to this image. */
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
    gpio_clear(GPIOA, GPIO12);
    delay_ms(20);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO12);
    rcc_periph_clock_enable(RCC_USB);
    usbd_device *device = usbd_init(&st_usbfs_v1_usb_driver, &device_descriptor,
        configurations, strings, 3, control_buffer, sizeof(control_buffer));
    usbd_register_set_config_callback(device, configuration_set);
    usbd_register_reset_callback(device, reset_received);
    usbd_register_control_callback(device, 0x01, 0x1f, hid_control);
    usbd_register_control_callback(device, 0x40, 0x7f, development_control);

    encoder_state encoder = {
        .phase = (gpio_get(GPIOB, GPIO12) ? 1 : 0) | (gpio_get(GPIOB, GPIO13) ? 2 : 0),
        .fraction = 0,
    };
    bool candidate_press = false;
    uint32_t candidate_since = milliseconds;
    bool boot_button_down = false;
    uint32_t boot_since = 0;
    while (1) {
        usbd_poll(device);
        if (boot_requested) enter_bootloader();
        uint8_t next = (gpio_get(GPIOB, GPIO12) ? 1 : 0) | (gpio_get(GPIOB, GPIO13) ? 2 : 0);
        int8_t encoder_step = encoder_read_step(&encoder, next);
        int32_t counts = encoder_step * ELEMENT_MF_JOG_COUNTS_PER_STEP;
        if (configured && counts && pending_counts + counts <= 32767 &&
            pending_counts + counts >= -32767) {
            pending_counts += counts;
        }

        bool raw_press = !gpio_get(GPIOA, GPIO15);
        if (raw_press != candidate_press) {
            candidate_press = raw_press;
            candidate_since = milliseconds;
        }
        if (pressed != candidate_press && (uint32_t)(milliseconds - candidate_since) >= 5) {
            pressed = candidate_press;
        }
        if (configured) {
            uint8_t events[2];
            unsigned count = transport_gesture_update(&gesture, pressed, milliseconds, events);
            queue_button_events(events, count);
        }

        bool small_down = !gpio_get(GPIOA, GPIO7);
        if (small_down && !boot_button_down) boot_since = milliseconds;
        if (small_down && (uint32_t)(milliseconds - boot_since) >= 2000) enter_bootloader();
        boot_button_down = small_down;

        if (configured && (pending_counts || queue_read != queue_write)) {
            bool queued = queue_read != queue_write;
            uint8_t report_buttons = queued ? button_queue[queue_read] : sent_buttons;
            int8_t step = pending_counts > 127 ? 127 : pending_counts < -128 ? -128 : pending_counts;
            uint8_t report[15];
            element_mf_transport_report(report, report_buttons, step);
            if (usbd_ep_write_packet(device, 0x81, report, sizeof(report)) == sizeof(report)) {
                pending_counts -= step;
                if (queued) queue_read = (queue_read + 1) % 16;
                sent_buttons = report_buttons;
                memcpy(last_report, report, sizeof(report));
            }
        }
    }
}


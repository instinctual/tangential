/* Exercise the actual firmware's descriptor/control callbacks on the host.
 * Hardware-dependent entry points are discarded by the linker. */
#define main unused_firmware_main
#include "../src/firmware/main.c"
#undef main
#include <assert.h>
#include <stdio.h>

static usbd_control_callback installed_control;
static usbd_control_callback installed_development_control;
static unsigned endpoints_configured;
static uint16_t output_a;

int usbd_register_control_callback(usbd_device *dev, uint8_t type, uint8_t mask,
                                  usbd_control_callback callback) {
    (void)dev;
    if (type == 1 && mask == 0x1f) installed_control = callback;
    else if (type == 0x40 && mask == 0x7f) installed_development_control = callback;
    else assert(false);
    return 0;
}

void usbd_ep_setup(usbd_device *dev, uint8_t address, uint8_t type, uint16_t size,
                   usbd_endpoint_callback callback) {
    (void)dev;
    assert(type == USB_ENDPOINT_ATTR_INTERRUPT);
    /* Reserve complete PMA words, including for the odd-length IN report. */
    assert((address == 0x81 && size == 16 && callback == NULL) ||
           (address == 0x01 && size == 64 && callback == output_received));
    endpoints_configured++;
}

void gpio_set(uint32_t port, uint16_t pins) { if (port == GPIOA) output_a |= pins; }
void gpio_clear(uint32_t port, uint16_t pins) { if (port == GPIOA) output_a &= ~pins; }
uint16_t gpio_get(uint32_t port, uint16_t pins) {
    return (port == GPIOA ? 0xff7f : 0xfeff) & pins;
}
uint16_t usbd_ep_read_packet(usbd_device *dev, uint8_t address, void *buffer, uint16_t length) {
    (void)dev; (void)address; (void)buffer; (void)length;
    return 0;
}

int main(void) {
    const uint8_t captured[] = {
        0x05,0x01,0x09,0x08,0xa1,0x01,0x05,0xf0,0x09,0x01,0x75,0x08,
        0x95,0x0f,0x81,0x02,0x05,0xf0,0x09,0x02,0x75,0x08,0x95,0x40,
        0x91,0x02,0xc0
    };
    assert(device_descriptor.idVendor == 0x04d8 && device_descriptor.idProduct == 0xfa1a);
    assert(device_descriptor.bMaxPacketSize0 == 8 && device_descriptor.bNumConfigurations == 2);
    assert(configurations[0].bConfigurationValue == 1 && configurations[1].bConfigurationValue == 2);
    assert(endpoints[0].bInterval == 20 && endpoints[1].bInterval == 16);
    assert(endpoints[0].wMaxPacketSize == 15 && endpoints[1].wMaxPacketSize == 64);
    for (uint16_t config = 1; config <= 2; config++) {
        installed_control = NULL;
        installed_development_control = NULL;
        endpoints_configured = 0;
        configuration_set(NULL, config);
        assert(configured && installed_control && installed_development_control && endpoints_configured == 2);
        struct usb_setup_data request = {.bmRequestType = 0x81, .bRequest = USB_REQ_GET_DESCRIPTOR,
                                         .wValue = 0x2200, .wIndex = 0, .wLength = 27};
        uint8_t *buffer = NULL;
        uint16_t length = 128;
        assert(installed_control(NULL, &request, &buffer, &length, NULL) == USBD_REQ_HANDLED);
        assert(length == sizeof(captured) && memcmp(buffer, captured, length) == 0);
        length = 8;
        assert(installed_control(NULL, &request, &buffer, &length, NULL) == USBD_REQ_HANDLED && length == 8);
        request.bmRequestType = 0x21;
        request.bRequest = 0x0a;
        request.wValue = 0;
        assert(installed_control(NULL, &request, &buffer, &length, NULL) == USBD_REQ_HANDLED);
        request.wIndex = 1;
        assert(installed_control(NULL, &request, &buffer, &length, NULL) == USBD_REQ_NEXT_CALLBACK);

        request = (struct usb_setup_data){.bmRequestType=0xc0, .bRequest=0x54,
                                         .wValue=1, .wIndex=0x5831, .wLength=12};
        length = 12;
        assert(installed_development_control(NULL, &request, &buffer, &length, NULL) == USBD_REQ_HANDLED);
        assert(length == 12 && buffer[0] == 0x7f && buffer[1] == 0xff && buffer[10] == 11 && buffer[11] == 1);
        usbd_control_complete_callback complete = NULL;
        boot_requested = false;
        request = (struct usb_setup_data){.bmRequestType=0x40, .bRequest=0x54,
                                         .wValue=0x424c, .wIndex=0x5831, .wLength=0};
        assert(installed_development_control(NULL, &request, &buffer, &length, &complete) == USBD_REQ_HANDLED);
        assert(!boot_requested && complete);
        complete(NULL, &request);
        assert(boot_requested);
        request.wIndex = 0;
        assert(installed_development_control(NULL, &request, &buffer, &length, &complete) == USBD_REQ_NEXT_CALLBACK);
    }
    assert(selected_page == 0);
    const uint16_t indicators = GPIO3 | GPIO4 | GPIO5 | GPIO6;
    for (uint8_t page = 0; page < TRANSPORT_PAGE_COUNT; page++) {
        assert(selected_page == page);
        assert((output_a & indicators) == (GPIO6 >> page));
        pending_counts = 100;
        select_next_page();
        assert(pending_counts == 0);
    }
    assert(selected_page == 0);
    select_next_page();
    configuration_set(NULL, 2); /* USB reconfiguration preserves orientation. */
    assert(selected_page == 1 && (output_a & indicators) == GPIO5);
    reset_received();
    assert(selected_page == 1 && !(output_a & indicators));
    configuration_set(NULL, 1);
    assert(selected_page == 1 && (output_a & indicators) == GPIO5);
    configuration_set(NULL, 0);
    assert(!configured);
    queue_read = queue_write = 0;
    const uint8_t tap[] = {ELEMENT_MF_PLAY_BUTTON, 0};
    const uint8_t stop[] = {ELEMENT_MF_STOP_BUTTON};
    const uint8_t release[] = {0};
    for (unsigned i = 0; i < 7; i++) queue_button_events(tap, 2);
    assert(queue_read == 0 && queue_write == 14);
    queue_button_events(tap, 2); /* No room for both edges: drop the whole tap. */
    assert(queue_write == 14);
    queue_button_events(stop, 1); /* Reserve space for the later release. */
    assert(queue_write == 14);
    queue_button_events(release, 1);
    assert(queue_write == 15 && button_queue[14] == 0);
    queue_button_events(release, 1); /* A full queue cannot suppress release. */
    assert(queue_read == 0 && queue_write == 1 && button_queue[0] == 0);
    puts("Firmware USB descriptor and reconfiguration tests passed");
    return 0;
}

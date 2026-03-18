#include "a2_usb_core.h"

#define USB_MAX_DESC 512  // big enough for most config descriptor trees

// stash the CDC control interface number during parsing
// the CDC ACM code uses this to send class requests 
static uint8_t g_cdc_ctrl_if = 0; // remember which interface is CDC control

uint8_t usb_cdc_ctrl_interface(void) {
    // tiny accessor so drivers do not poke globals
    return g_cdc_ctrl_if;
}

void usb_init(void) {
    // basic initialization sequence for usb
    //  1) start in address 0
    //  2) set default EP0 max packet size
    //  3) init host controller + reset port
    usb_set_device_address(0);
    usb_set_ep0_mps(8);

    // host controller bring-up
    dwc2_init();
    dwc2_wait_for_device();
    dwc2_port_reset();

    // speed comes from the port status bits
    usb_set_speed(dwc2_get_port_speed());
}

static uint16_t le16(const uint8_t* p) {
    // USB descriptors are little-endian byte arrays
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

int usb_enumerate(void) {
    // simple version of usb: assumes only one USB device is plugged into the pi!!!
    
    // we reuse the same setup packet buffer for each request
    struct usb_setup setup;

    // Step 1: read first 8 bytes of device descriptor so we learn EP0 MPS
    uint8_t dev_hdr[8];
    setup.bmRequestType = 0x80;
    setup.bRequest = 6;
    setup.wValue = 0x0100;
    setup.wIndex = 0;
    setup.wLength = sizeof(dev_hdr);
    if (usb_control_transfer(&setup, dev_hdr) < 0)
        return -1;

    // bMaxPacketSize0 lives at byte 7
    uint8_t ep0_mps = dev_hdr[7];
    usb_set_ep0_mps(ep0_mps);

    // Step 2: set device address
    // request address 1 and then wait for the device to latch it
    setup.bmRequestType = 0x00;
    setup.bRequest = 5; // SET_ADDRESS
    setup.wValue = 1;
    setup.wIndex = 0;
    setup.wLength = 0;
    if (usb_control_transfer(&setup, 0) < 0)
        return -1;
    delay_ms(10); // spec says wait at least 2ms, we are extra safe
    usb_set_device_address(1);

    // Step 3: read full device descriptor now that EP0 MPS is correct
    struct usb_device_descriptor dev_desc;
    setup.bmRequestType = 0x80;
    setup.bRequest = 6;
    setup.wValue = 0x0100;
    setup.wIndex = 0;
    setup.wLength = sizeof(dev_desc);
    if (usb_control_transfer(&setup, &dev_desc) < 0)
        return -1;

    // Step 4: read config descriptor header to get total length
    // this header is only 9 bytes
    uint8_t cfg_hdr[9];
    setup.bmRequestType = 0x80;
    setup.bRequest = 6;
    setup.wValue = 0x0200;
    setup.wIndex = 0;
    setup.wLength = sizeof(cfg_hdr);
    if (usb_control_transfer(&setup, cfg_hdr) < 0)
        return -1;

    // wTotalLength gives the full size of config + interface + endpoint descriptors
    uint16_t total_len = le16(&cfg_hdr[2]);
    if (total_len > USB_MAX_DESC)
        total_len = USB_MAX_DESC;

    // keep this static so we do not blow stack
    static uint8_t cfg_buf[USB_MAX_DESC];
    // read the whole descriptor tree in one control transfer
    setup.wLength = total_len;
    if (usb_control_transfer(&setup, cfg_buf) < 0)
        return -1;

    // the first bytes of cfg_buf are always the config descriptor
    struct usb_config_descriptor *cfg = (struct usb_config_descriptor *)cfg_buf;
    uint8_t cfg_value = cfg->bConfigurationValue;

    // Step 5: parse interfaces/endpoints for CDC-ACM data interface
    // NOTE: CDC has two interfaces: control (class 0x02) and data (class 0x0A)
    // track current interface class so we know how to interpret endpoints
    uint8_t cur_class = 0xFF;
    uint8_t bulk_in_ep = 0;
    uint8_t bulk_out_ep = 0;
    uint16_t bulk_mps = 64;

    // walk the descriptor list by length fields
    for (uint16_t idx = 0; idx + 2 <= total_len;) {
        uint8_t len = cfg_buf[idx];
        uint8_t type = cfg_buf[idx + 1];
        // descriptor length is always at least 2 bytes
        if (len < 2)
            break;

        if (type == USB_DESC_INTERFACE) {
            // interface descriptor changes current class context
            struct usb_interface_descriptor *ifd = (struct usb_interface_descriptor *)(cfg_buf + idx);
            cur_class = ifd->bInterfaceClass;
            if (ifd->bInterfaceClass == 0x02)
                g_cdc_ctrl_if = ifd->bInterfaceNumber;
        } else if (type == USB_DESC_ENDPOINT) {
            if (cur_class == 0x0A) {
                // endpoints for CDC data interface are bulk in and bulk out
                struct usb_endpoint_descriptor *epd = (struct usb_endpoint_descriptor *)(cfg_buf + idx);
                if ((epd->bmAttributes & 0x3) == EP_TYPE_BULK) {
                    uint8_t ep = epd->bEndpointAddress & 0x0F;
                    if (epd->bEndpointAddress & 0x80)
                        bulk_in_ep = ep;
                    else
                        bulk_out_ep = ep;
                    bulk_mps = epd->wMaxPacketSize & 0x7FF;
                }
            }
        }

        // advance by the descriptor length
        idx += len;
    }

    // Step 6: set configuration (activates the endpoints we just found)
    setup.bmRequestType = 0x00;
    setup.bRequest = 9; // SET_CONFIGURATION
    setup.wValue = cfg_value;
    setup.wIndex = 0;
    setup.wLength = 0;
    if (usb_control_transfer(&setup, 0) < 0)
        return -1;

    // if either bulk endpoint is missing, bail
    if (bulk_in_ep == 0 || bulk_out_ep == 0)
        return -1;

    // stash the endpoints so the CDC driver can use them
    usb_set_bulk_endpoints(bulk_in_ep, bulk_out_ep, bulk_mps);
    return 0;
}

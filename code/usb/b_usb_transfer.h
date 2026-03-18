#ifndef USB_TRANSFER_H
#define USB_TRANSFER_H

#include "rpi.h"
#include "c_dwc2.h"

// Setup packet layout for control transfers
// this struct is sent verbatim in the setup stage
struct usb_setup {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed, aligned(4)));

// configure address, EP0 MPS, link speed
void usb_set_device_address(uint32_t addr);
void usb_set_ep0_mps(uint32_t mps);
void usb_set_speed(uint32_t speed);
// stash bulk endpoints + max packet size for CDC data transfers
void usb_set_bulk_endpoints(uint32_t in_ep, uint32_t out_ep, uint32_t mps);

// control transfer on EP0
// returns bytes transferred or -1
int usb_control_transfer(const struct usb_setup *setup, void *data);
// bulk transfer helpers used by cdc_acm
int usb_bulk_in(void* buf, uint32_t len);
int usb_bulk_out(const void* buf, uint32_t len);

#endif

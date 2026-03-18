#ifndef USB_CORE_H
#define USB_CORE_H

#include "b_usb_transfer.h"

// Descriptor types (USB spec Table 9-5)
#define USB_DESC_DEVICE        1
#define USB_DESC_CONFIG        2
#define USB_DESC_INTERFACE     4
#define USB_DESC_ENDPOINT      5

// USB device descriptor layout, 18 bytes total
// from tianle docs
struct usb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed));

// from tianle docs
struct usb_config_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} __attribute__((packed));

// Interface descriptor
// we will use 2 interfaces: one for control and one for data
struct usb_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed));

// Endpoint descriptor: from tianle docs
// used to find bulk in and bulk out for CDC data
struct usb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed));

// initialize host controller + port
void usb_init(void);
// enumerate a single device and configure CDC-ACM endpoints
int usb_enumerate(void);

// Accessors for CDC class driver
uint8_t usb_cdc_ctrl_interface(void);

#endif

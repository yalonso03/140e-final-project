#include "rpi.h"
#include "usb/a2_usb_core.h"
#include "usb/a1_cdc_acm.h"

void notmain(void) {
    uart_init();
    printk("USB enum test: start\n");

    usb_init();
    if (usb_enumerate() < 0) {
        printk("ERROR: usb_enumerate failed\n");
        while (1)
            ;
    }

    if (cdc_init() < 0) {
        printk("ERROR: cdc_init failed\n");
        while (1)
            ;
    }

    printk("SUCCESS: CDC-ACM ready\n");
    while (1)
        ;
}

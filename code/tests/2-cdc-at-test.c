#include "rpi.h"
#include "usb/a2_usb_core.h"
#include "usb/a1_cdc_acm.h"

static void dump_bytes(const char* buf, int n) {
    for (int i = 0; i < n; i++)
        uart_put8((uint8_t)buf[i]);
}

void notmain(void) {
    uart_init();
    printk("CDC AT test: start\n");

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

    printk("CDC ready, sending AT\r\n");
    cdc_write("AT\r", 3);

    char buf[256];
    int total = 0;
    for (unsigned tries = 0; tries < 200; tries++) {
        int n = cdc_read(buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            printk("RX (%d): ", n);
            dump_bytes(buf, n);
            printk("\n");
            total += n;
        }
        delay_ms(10);
    }

    if (total == 0)
        printk("WARN: no response received\n");
    else
        printk("SUCCESS: received %d bytes\n", total);

    while (1)
        ;
}

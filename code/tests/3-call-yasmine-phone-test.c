#include "rpi.h"
#include "usb/a2_usb_core.h"
#include "usb/a1_cdc_acm.h"

static void dump_bytes(const char* buf, int n) {
    for (int i = 0; i < n; i++)
        uart_put8((uint8_t)buf[i]);
}

static void drain_rx(unsigned ms_total) {
    char buf[256];
    unsigned elapsed = 0;
    unsigned ticks = 0;
    while (elapsed < ms_total) {
        int n = cdc_read(buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            printk("RX (%d): ", n);
            dump_bytes(buf, n);
            printk("\n");
        }
        delay_ms(10);
        elapsed += 10;
        ticks++;
        if ((ticks % 10) == 0)
            printk(".");
    }
    printk("\n");
}

static void send_cmd(const char* cmd) {
    printk("TX: %s\n", cmd);
    cdc_write(cmd, (unsigned)strlen(cmd));
    cdc_write("\r", 1);
    drain_rx(10);  // drain the receive buffer for 500ms to be safe
    //! todo remove/shorten this draining i don't think it's necessary just wanted to be safe
}

void notmain(void) {
    uart_init();  // just so i can print stuff to console to be able to debug. not necessary 
    printk("STARTING DIALING TEST \n");

    usb_init();
    if (usb_enumerate() < 0) {
        panic("usb enumerate failed :(");
    }

    if (cdc_init() < 0) {
        panic("cdc init failed :((((");
    }

    printk("CDC all set!!! usb_init() worked successfully :-)\n");
    printk("starting to send commands from pi --> modem \n");
    send_cmd("AT&F");
    send_cmd("ATE0");
    send_cmd("AT+FCLASS=8");
    send_cmd("AT+VLS=0");
    printk("dialing yasmine's phone number!");
    send_cmd("ATDT6508043008");

    // to be safe wait in case anything else were to come up 
    drain_rx(5000);

    printk("Done\n");
    while (1)
        ;
}

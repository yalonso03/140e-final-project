#include "rpi.h"
#include "sw-uart.h"
#include "cycle-count.h"
#include "cycle-util.h"

#include <stdarg.h>

void busywait_n_cycles(uint32_t start_n_cycles, uint32_t n_cycles_to_wait) {
    while (cycle_cnt_read() < start_n_cycles + n_cycles_to_wait) {
        ;
    }
}


// bit bang the 8-bits <b> to the sw-uart <uart>.
//  - at 115200 baud you can probably just use microseconds,
//    but at faster rates you'd want to use cycles.
//  - libpi/include/cycle-util.h has some helper 
//    that you can use if you want (don't have to).
//
// recall: 
//    - the microseconds to write each bit (0 or 1) is in 
//      <uart->usec_per_bit>
//    - the cycles to write each bit (0 or 1) is in 
//      <uart->cycle_per_bit>
//    - <cycle_cnt_read()> counts cycles
//    - <timer_get_usec()> counts microseconds.
void sw_uart_put8(sw_uart_t *uart, uint8_t b) {
    // use local variables to minimize any loads or stores
    int tx = uart->tx;
    uint32_t n = uart->cycle_per_bit,
             u = n,
             s = cycle_cnt_read();

    // (1) write start bit for at least n 
    gpio_set_off(tx);
    busywait_n_cycles(s, n);

    // (2) Transmit the 8 bits, each one for n, going LSB to MSB
    for (uint32_t j = 0; j < 8; j++) {
        s = cycle_cnt_read();
        if (b & 1) {  // LSB is a 1
            // s = cycle_cnt_read();
            gpio_set_on(tx);
        } else {  // LSB is a 0
            // s = cycle_cnt_read();
            gpio_set_off(tx);
        }
        busywait_n_cycles(s, n);
        b = b >> 1;
    }

    // (3) Write stop bit (1) for at least n cycles
    s = cycle_cnt_read();
    gpio_set_on(tx);
    busywait_n_cycles(s, n);
}

// optional: do receive.
//      EASY BUG: if you are reading input, but you do not get here in 
//      time it will disappear.
int sw_uart_get8_timeout(sw_uart_t *uart, uint32_t timeout_usec) {
    unsigned rx = uart->rx;

    // right away (used to be return never).
    while(!wait_until_usec(rx, 0, timeout_usec))
        return -1;

    todo("implement this code\n");
}

// finish implementing this routine.  
sw_uart_t sw_uart_init_helper(unsigned tx, unsigned rx,
        unsigned baud, 
        unsigned cyc_per_bit,
        unsigned usec_per_bit) 
{
    // remedial sanity checking
    assert(tx && tx<31);
    assert(rx && rx<31);
    assert(cyc_per_bit && cyc_per_bit > usec_per_bit);
    assert(usec_per_bit);

    // basic sanity checking.  if this fails lmk
    unsigned mhz = 700 * 1000 * 1000;
    unsigned derived = cyc_per_bit * baud;
    if(! ((mhz - baud) <= derived && derived <= (mhz + baud)) )
        panic("too much diff: cyc_per_bit = %d * baud = %d\n", 
            cyc_per_bit, cyc_per_bit * baud);

    // make sure you set TX to its correct default!
    
    gpio_set_output(tx);
    gpio_set_on(tx);
    gpio_set_input(rx);
    //todo("setup rx,tx and initial state of tx pin.");

    return (sw_uart_t) { 
            .tx = tx, 
            .rx = rx, 
            .baud = baud, 
            .cycle_per_bit = cyc_per_bit ,
            .usec_per_bit = usec_per_bit 
    };
}

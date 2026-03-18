// simple mini-uart driver: implement every routine 
// with a <todo>.
//
// NOTE: 
//  - from broadcom: if you are writing to different 
//    devices you MUST use a dev_barrier().   
//  - its not always clear when X and Y are different
//    devices.
//  - pay attenton for errata!   there are some serious
//    ones here.  if you have a week free you'd learn 
//    alot figuring out what these are (esp hard given
//    the lack of printing) but you'd learn alot, and
//    definitely have new-found respect to the pioneers
//    that worked out the bcm eratta.
//
// historically a problem with writing UART code for
// this class (and for human history) is that when 
// things go wrong you can't print since doing so uses
// uart.  thus, debugging is very old school circa
// 1950s, which modern brains arne't built for out of
// the box.   you have two options:
//  1. think hard.  we recommend this.
//  2. use the included bit-banging sw uart routine
//     to print.   this makes things much easier.
//     but if you do make sure you delete it at the 
//     end, otherwise your GPIO will be in a bad state.
//
// in either case, in the next part of the lab you'll
// implement bit-banged UART yourself.
#include "rpi.h"

// change "1" to "0" if you want to comment out
// the entire block.
#if 1
//*****************************************************
// We provide a bit-banged version of UART for debugging
// your UART code.  delete when done!
//
// NOTE: if you call <emergency_printk>, it takes 
// over the UART GPIO pins (14,15). Thus, your UART 
// GPIO initialization will get destroyed.  Do not 
// forget!   

// header in <libpi/include/sw-uart.h>
#include "sw-uart.h"
static sw_uart_t sw_uart;

// a sw-uart putc implementation.
static int sw_uart_putc(int chr) {
    sw_uart_put8(&sw_uart,chr);
    return chr;
}

// call this routine to print stuff. 
//
// note the function pointer hack: after you call it 
// once can call the regular printk etc.
__attribute__((noreturn)) 
static void emergency_printk(const char *fmt, ...)  {
    // we forcibly initialize in case the 
    // GPIO got reset. this will setup 
    // gpio 14,15 for sw-uart.
    sw_uart = sw_uart_default();

    // all libpi output is via a <putc>
    // function pointer: this installs ours
    // instead of the default
    rpi_putchar_set(sw_uart_putc);

    // do print
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);

    // at this point UART is all messed up b/c we took it over
    // so just reboot.   we've set the putchar so this will work
    clean_reboot();
}

#undef todo
#define todo(msg) do {                          \
    emergency_printk("%s:%d:%s\nDONE!!!\n",     \
            __FUNCTION__,__LINE__,msg);         \
} while(0)

// END of the bit bang code.
#endif


enum {

    AUX_ENABLES = 0x20215004,
    AUX_MU_CNTL_REG = 0x20215060,
    AUX_MU_IIR_REG = 0x20215048,
    AUX_MU_IER_REG = 0x20215044,
    AUX_MU_LCR_REG = 0x2021504C,
    AUX_MU_IO_REG = 0x20215040,          // page 11
    AUX_MU_BAUD_REG = 0x20215068,        // page 19
    AUX_MU_LSR_REG = 0x20215054,

};


//*****************************************************
// the rest you should implement.

// called first to setup uart to 8n1 115200  baud,
// no interrupts.
//  - you will need memory barriers, use <dev_barrier()>
//
//  later: should add an init that takes a baud rate.
void uart_init(void) {
    dev_barrier();
    // 0) GPIO pins RX and TX (pins 14 and 15) must be configured!!!!
    gpio_set_function(GPIO_TX, GPIO_FUNC_ALT5);  // /libpi/include/gpio.h
    gpio_set_function(GPIO_RX, GPIO_FUNC_ALT5);

    dev_barrier();

    // 1) You need to turn on the UART in AUX. Make sure you read-modify-write --- don't kill the SPIm enables.
    unsigned auxenb_value = GET32(AUX_ENABLES) & 0b111;  // Page 9, masking off the upper 31:3 because we don't care
    auxenb_value |= 0b1;
    PUT32(AUX_ENABLES, auxenb_value);              // Set lowest bit of the AUX_ENABLES register to be a 1

    dev_barrier();  // large gap in mem addys seems to imply different devices ???

    // 2) Immediately disable tx/rx (you don't want to send garbage). Page 17
    unsigned ctrl_reg_value = GET32(AUX_MU_CNTL_REG) & 0xFF;     // Page 17, getting the og AUX_MU_CNTL_REG, only care about lower 8 bits
    ctrl_reg_value = ctrl_reg_value & ~(0b1111);
    PUT32(AUX_MU_CNTL_REG, ctrl_reg_value);                  // set lowest 4 bits (bit 3: flow control, bit 2: flow control, bit 1: transmitter enable and bit 0: receiver enable) to 0, keep all of the rest as 1

    // 3) Figure out which registers you can ignore (e.g., IO, p 11). Many devices have many registers you can skip.

    // 4b) Disable interrupts. For safety, write page 12
        // reordered this to write in ascending order of registers
    PUT32(AUX_MU_IER_REG, 0b00);  // set lower two bits to disable recv/transmit interrupts

    // 4a) Find and clear all parts of its state (e.g., FIFO queues) since we are not absolutely positive they do not hold garbage.
    PUT32(AUX_MU_IIR_REG, 0b110);     // Page 13, care about bits 1 and 2 write 1 to clear the rx/tx FIFO queues. We're also writing a 0 to the lowest bit cuz it doesn't matter

    unsigned lcr_reg = GET32(AUX_MU_LCR_REG) & 0xff;  // page 14

    // set the DLAB access bit (bit 7) to be 0 for extra safety, keep other remaining lower 7 bits (pg 14)
    lcr_reg = lcr_reg & ~(0b1 << 7);

    // set the data size lowest 2 bits to be 1 for UART 8bit mode (pg 14)
    lcr_reg = lcr_reg | 0b11;

    PUT32(AUX_MU_LCR_REG, lcr_reg);  // write it 


    // 5) Configure: 115200 Baud, 8 bits, 1 start bit, 1 stop bit. No flow control. -- flow control handled in step 2 with the AUX_MU_CNTL_REG write

    // Baudrate_reg calculation based on page 11 equation: 
        // baudrate_reg = system_clk_freq/baudrate * 1/8 - 1
        // system_clk_freq = 250MHz
        // baudrate = 115200
        // --> baudrate_reg = 270.267, round down to 270


    // Set the calculated baudrate_reg value to 270 per the above calculation, page 19
    PUT32(AUX_MU_BAUD_REG, 270);


    // // For when system_clk_freq is now 1000MHz (post lab-9 overclocking), recalculate baudrate_reg
    // unsigned baudrate_reg_val = (system_clk_freq / 115200) * (1/8)
    // PUT32(AUX_MU_BAUD_REG, 650);

    // 6) Enable tx/rx. It should be working!
    ctrl_reg_value = GET32(AUX_MU_CNTL_REG) & 0xFF;     // Page 17, get the AUX_MU_CNTL_REG value again
    ctrl_reg_value = ctrl_reg_value | 0b11;
    PUT32(AUX_MU_CNTL_REG, ctrl_reg_value);            // set bits 0 and 1 to high to re-enable tx/rx

    // NOTE: for cross-checking: make sure write UART 
    // addresses in order
    dev_barrier();
}

// disable the uart: make sure all bytes have been ???? sent????
// 
void uart_disable(void) {
    dev_barrier();
    // TODO: implement this!
    // I think we want to ensure that all bytes have been sent, meaning the transmitter is idle
    // can be checked thru AUX_MU_LSR_REG on page 15, specificlaly bit 6, which indicates transmit FIFO empty and transmitter idle
    while((GET32(AUX_MU_LSR_REG) & (1 << 6)) == 0) {
        ;
    }

    dev_barrier();

    unsigned auxenb_value = GET32(AUX_ENABLES) & 0b111;  // Page 9, masking off the upper 31:3 because we don't care
    auxenb_value = auxenb_value & ~(0b1);
    PUT32(AUX_ENABLES, auxenb_value);              // Set lowest bit of the AUX_ENABLES register to be a 0
    dev_barrier();
}

// returns one byte from the RX (input) hardware
// FIFO.  if FIFO is empty, blocks until there is 
// at least one byte.
int uart_get8(void) {
    // blocks until it can read a byte from the mini-UART, and returns the byte as a signed int (for sort-of consistency with getc).


    dev_barrier();
    // Use bit 0, data ready field, to busy wait so long as the FIFO doesn't hold any symbols
    while((GET32(AUX_MU_LSR_REG) & 0b1) == 0) {
        ;
    }
    unsigned result = GET32(AUX_MU_IO_REG) & 0xFF;  // or int idk if it matters
    dev_barrier();
    return result;
}



// returns 1 if the hardware TX (output) FIFO has room
// for at least one byte.  returns 0 otherwise.
int uart_can_put8(void) {
    dev_barrier();
    unsigned can_transmit_fifo_accept_at_least_one_byte = (GET32(AUX_MU_LSR_REG));  // transmitter empty bit (bit 5) page 15
    can_transmit_fifo_accept_at_least_one_byte &= (1 << 5);
    can_transmit_fifo_accept_at_least_one_byte >>= 5; 
    dev_barrier();
    return can_transmit_fifo_accept_at_least_one_byte;
}

// put one byte on the TX FIFO, if necessary, waits
// until the FIFO has space.
int uart_put8(uint8_t c) {
    // TODO: implement this!
    dev_barrier();

    // While there's NOT space in the tx fifo to accept at least one byte, busy wait
    while ((GET32(AUX_MU_LSR_REG) & (0b100000)) == 0) {
        ;
    }

    // Then put the c in the fifo using IO REG page 11
    PUT32(AUX_MU_IO_REG, c);

    dev_barrier();
    return 1;
}

// returns:
//  - 1 if at least one byte on the hardware RX FIFO.
//  - 0 otherwise
int uart_has_data(void) {
    dev_barrier();
    unsigned recv_fifo_holds_at_least_one_byte = GET32(AUX_MU_LSR_REG) & 0b1;
    dev_barrier();
    return recv_fifo_holds_at_least_one_byte;
}

// returns:
//  -1 if no data on the RX FIFO.
//  otherwise reads a byte and returns it.
int uart_get8_async(void) { 
    if(!uart_has_data())
        return -1;
    return uart_get8();
}

// returns:
//  - 1 if TX FIFO empty AND idle.
//  - 0 if not empty.
int uart_tx_is_empty(void) {
    dev_barrier();
    unsigned transmitter_idle_bit = GET32(AUX_MU_LSR_REG) & 0b1000000;  // Transmitter idle bit page 15
    dev_barrier();
    return transmitter_idle_bit >> 6;
}

// return only when the TX FIFO is empty AND the
// TX transmitter is idle.  
//
// used when rebooting or turning off the UART to
// make sure that any output has been completely 
// transmitted.  otherwise can get truncated 
// if reboot happens before all bytes have been
// received.
void uart_flush_tx(void) {
    while(!uart_tx_is_empty())
        rpi_wait();
}

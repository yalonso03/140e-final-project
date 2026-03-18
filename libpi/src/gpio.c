/*
 * Implement the following routines to set GPIO pins to input or 
 * output, and to read (input) and write (output) them.
 *  1. DO NOT USE loads and stores directly: only use GET32 and 
 *    PUT32 to read and write memory.  See <start.S> for thier
 *    definitions.
 *  2. DO USE the minimum number of such calls.
 * (Both of these matter for the next lab.)
 *
 * See <rpi.h> in this directory for the definitions.
 *  - we use <gpio_panic> to try to catch errors.  For lab 2
 *    it only infinite loops since we don't have <printk>
 */
#include "rpi.h"

// weird panic: we just infinite loop since we don't have 
// printk in 2-gpio.
void gpio_panic(const char *msg, ...) {
    while(1);
}

// See broadcomm documents for magic addresses and magic values.
//
// If you pass addresses as:
//  - pointers use put32/get32.
//  - integers: use PUT32/GET32.
//  semantics are the same.
enum {
    // Max gpio pin number.
    GPIO_MAX_PIN = 53,

    GPIO_BASE = 0x20200000,
    gpio_set0  = (GPIO_BASE + 0x1C),
    gpio_clr0  = (GPIO_BASE + 0x28),
    gpio_lev0  = (GPIO_BASE + 0x34)

    // <you will need other values from BCM2835!>
};

// ADDED FROM LAB 
// set GPIO function for <pin> (input, output, alt...).
// settings for other pins should be unchanged.

// Only modify your gpio.c you copied from Lab 2 to 1-fake-pi.
// Adapt your gpio_set_output code to bitwise-or the given flag.
// Error check both the input pin and the function value.

// JACOBS
// void gpio_set_function(unsigned pin, gpio_func_t function) {
//     if(pin > GPIO_MAX_PIN)
//         gpio_panic("illegal pin=%d\n", pin);

//     if (function > 0b111)
//         gpio_panic("illegal function=%d\n", function);

//     // Implement this.
//     unsigned fn = pin / 10;
//     unsigned idx = pin % 10;
//     unsigned addr = GPIO_BASE + 4 * fn;
    
//     unsigned value = GET32(addr);
//     value &= ~(0x7 << (idx * 3));
//     value |= function << (idx * 3);
//     PUT32(addr, value);
// }

// YASMINE's
void gpio_set_function(unsigned pin, gpio_func_t function) {
    if (pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    // Implement this.
    if((function & 0b111) != function)
        // NOTE: it says "func" not "function" and uses "%x" not "%d"
        gpio_panic("illegal func=%x\n", function);

    unsigned fn_select = pin / 10;                        // which GPIO Function Select Register (which n for GPFSELn) -- page 90
    unsigned gpfseln_addr = GPIO_BASE + (fn_select * 4);  // address of the above register -- pg 90
    unsigned value = GET32(gpfseln_addr);                 // we use read-modify-write pattern here so as not to wipe out bits in the non-focus area (could be something relevant there)
    unsigned shift = (pin % 10) * 3;                      // Figure out the bits (e.g. 2-0 or 17-15) for the relevant FSEL field. Each FSEL field is 3 bits wide
    unsigned mask = 0x0000007;                            // 3-bit wide mask for the FSEL field 0b00..0111  (need to shift this mask over too)
    mask = mask << shift;
    value &= ~mask;                                       // ~mask is like 0b11111..1000 -- lets us keep everything else, but wipe out the bits in the field we're about to set 
    unsigned bits = function;                             // e.g. 0b001 for output, or 0b100 for alt fn 0, etc
    value |= bits << shift;                               // OR with the 0b001 (bits) to set to output
    PUT32(gpfseln_addr, value);

}


//
// Part 1 implement gpio_set_on, gpio_set_off, gpio_set_output
//

// set <pin> to be an output pin.
//
// NOTE: fsel0, fsel1, fsel2 are contiguous in memory, so you
// can (and should) use ptr calculations versus if-statements!
void gpio_set_output(unsigned pin) {
    gpio_set_function(pin, GPIO_FUNC_OUTPUT);
}

// Set GPIO <pin> = on.
void gpio_set_on(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    // Implement this. 
    // NOTE: 
    //  - If you want to be slick, you can exploit the fact that 
    //    SET0/SET1 are contiguous in memory.
    unsigned output_set_n = pin / 32; // either GPIO Pin Output set 0 or 1, on page 90
    unsigned gpsetn_addr = gpio_set0 + (output_set_n * 4);

    //value &= ~mask;
    unsigned shift = pin % 32;
    unsigned value = 0x1 << shift;
    PUT32(gpsetn_addr, value);
}

// Set GPIO <pin> = off
void gpio_set_off(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    // Implement this. 
    // NOTE: 
    //  - If you want to be slick, you can exploit the fact that 
    //    CLR0/CLR1 are contiguous in memory.
    unsigned output_clear_n = pin / 32; // either GPIO Pin Output set 0 or 1, on page 90
    unsigned gpsetn_addr = gpio_clr0 + (output_clear_n * 4);

    unsigned shift = pin % 32;
    unsigned value = 0x1 << shift;
    PUT32(gpsetn_addr, value);
}

// Set <pin> to <v> (v \in {0,1})
void gpio_write(unsigned pin, unsigned v) {
    if(v)
        gpio_set_on(pin);
    else
        gpio_set_off(pin);
}

//
// Part 2: implement gpio_set_input and gpio_read
//

// set <pin> = input.
void gpio_set_input(unsigned pin) {
    gpio_set_function(pin, GPIO_FUNC_INPUT);
}

// Return 1 if <pin> is on, 0 if not.
int gpio_read(unsigned pin) {
    unsigned v = 0;

    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    unsigned level_register_n = pin / 32;
    unsigned gplevn_addr = gpio_lev0 + (level_register_n * 4);

    unsigned mask = 0x1;
    unsigned shift = pin % 32;
    mask = mask << shift;
    unsigned value_from_slot = GET32(gplevn_addr);

    // This will have either a 1 somewhere or all 0s e.g. 000000100000000 or 000000000000000000
    unsigned result = mask & value_from_slot;
    if (result) {
        return 1;
    } else {
        return 0;
    }

    return v;
}



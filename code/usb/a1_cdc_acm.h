#ifndef CDC_ACM_H
#define CDC_ACM_H
#include "a2_usb_core.h"

// the cdc_acm (communications device class abstract control model) interface allows for the Pi to communicate serial
// data over USB. To our programs being run on the pi, it thinks it's just sending bytes. cdc_write and cdc_read then use bulk
// USB transfers in order to send phone commands over USB!
// to my pi, this CDC ACM code makes it look like the phone modem is a serial port. wahoo!

// NOTE: these helpers assume usb_enumerate() has already been called to pick CDC endpoints

// set up everything needed to be able to read and write to the modem via usb
// sends initial usb control requests
int cdc_init(void);

// uses usb bulk out to send data to the phone modem
void cdc_write(const char* buf, uint32_t len);

// uses usb bulk in to receive data from the phone modem
int cdc_read(char* buf, uint32_t max_len);

#endif

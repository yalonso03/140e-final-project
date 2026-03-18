#include "a1_cdc_acm.h"

// CDC line coding describes the serial line parameters the modem should use
struct cdc_line_coding {
    uint32_t baud_rate_bits_per_sec;  // baud rate in bits per second
    uint8_t stop_bit_setting;  // stop bits setting
    uint8_t parity_setting;  // parity setting
    uint8_t num_data_bits_per_frame;  // number of data bits per frame
} __attribute__((packed));


// set up everything!
// send initial usb control requests --> modem (SET_LINE_CODING and SET_CONTROL_LINE_STATE) -- defined by usb cdc specs
int cdc_init(void) {
    // get the interface number saved during enumeration
    // interface number identifies the CDC control interface in the config descriptor
    uint8_t cdc_ctrl_interface = usb_cdc_ctrl_interface();

    struct cdc_line_coding serial_line_params;
    // set up serial line
    // https://community.st.com/t5/stm32-mcus/how-to-set-usb-cdc-line-coding/ta-p/49349#:~:text=Line%20coding%20is%20used%20to,proposal%20in%20STM32CubeMX%20project%20repository.
    // "Line coding is used to transfer parameters of UART interface, which is emulated using USB CDC - Virtual COM port. In STM32 legacy USB library is line coding handled in file usbd_cdc_if.c, function CDC_Control_xS(FS for Full Speed USB, HS for High Speed USB). STM32CubeMX generates CDC_Control_xS function empty, you can find handling proposal in STM32CubeMX project repository."
    serial_line_params.baud_rate_bits_per_sec = 115200;
    serial_line_params.stop_bit_setting = 0; //1 stop bit
    serial_line_params.parity_setting = 0; // none
    serial_line_params.num_data_bits_per_frame = 8; // 8 data bits for 8N1

    struct usb_setup setup; // from tianle docs

    // SET_LINE_CODING tells the modem our UART parameters
    // build a class specific control request on the CDC control interface
    setup.bmRequestType = 0x21; // Class, Interface, Out
    setup.bRequest = 0x20;
    setup.wValue = 0; // unused for SET_LINE_CODING
    setup.wIndex = cdc_ctrl_interface; // which interface the request applies to
    setup.wLength = sizeof(serial_line_params); // size of the line coding struct
    if (usb_control_transfer(&setup, &serial_line_params) < 0)
        return -1;

    // SET_CONTROL_LINE_STATE raises DTR and RTS
    setup.bmRequestType = 0x21; // Class, Interface, Out for CDC control
    setup.bRequest = 0x22; // CDC request for control line state
    setup.wValue = 3; // bitmask enabling DTR and RTS
    setup.wIndex = cdc_ctrl_interface; // target the CDC control interface
    setup.wLength = 0; // no data stage for this request
    if (usb_control_transfer(&setup, 0) < 0)
        return -1;
    return 0;
}


// write bytes to the modem over CDC bulk OUT
void cdc_write(const char* buf, uint32_t len) {
    // split into 2048-byte chunks so we fit in the DMA buffer
    // usb_bulk_out uses the shared DMA buffer in usb_transfer.c
    uint32_t i = 0;

    // while we have more bytes to write:
    while (i < len) {
        uint32_t chunk_sz = len - i;

        // break into chunks of max size USB_DMA_BUF_SIZE (2048)
        if (chunk_sz > 2048) {
            chunk_sz = 2048;
        }

        // do a bulk usb transfer of the current chunk_sz bytes from the buffer 
        if (usb_bulk_out(buf + i, chunk_sz) < 0)
            return;
        i += chunk_sz;
    }
}


// read bytes from the modem over CDC bulk IN
int cdc_read(char* buf, uint32_t max_len) {
    if (max_len == 0) {return 0;}

    // chunk it up, we can handle max 2048 bytes
    if (max_len > 2048) {
        max_len = 2048;
    }
    // one bulk IN
    // caller should loop and call cdc_read if they want to continually read from buffer loop for more if they want streaming reads
    int r = usb_bulk_in(buf, max_len);
    if (r < 0) {
        return 0;
    }
    return r;
}

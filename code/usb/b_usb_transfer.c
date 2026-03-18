#include "b_usb_transfer.h"

#define USB_DMA_BUF_SIZE 2048  // matches USB FS max transfer we want to handle in one go

static uint8_t dma_buf[USB_DMA_BUF_SIZE] __attribute__((aligned(64))); // DMA needs cacheline alignment

// cached device state from enumeration
static uint32_t g_devaddr = 0;      // USB device address (set during enumeration)
static uint32_t g_ep0_mps = 8;      // control EP0 max packet size
static uint32_t g_speed = DWC2_SPEED_FS;
static uint32_t g_bulk_in_ep = 0;
static uint32_t g_bulk_out_ep = 0;
static uint32_t g_bulk_mps = 64;
static uint32_t g_bulk_pid_in = PID_DATA0;   // track data toggling 
static uint32_t g_bulk_pid_out = PID_DATA0; // track data toggling

static uint32_t pktcnt_for_len(uint32_t len, uint32_t mps) {
    // number of packets needed for <len> bytes at <mps> per packet
    if (len == 0)
        return 1;
    uint32_t cnt = 0;
    // split into max sized packets
    while (len) {
        cnt++;
        if (len > mps)
            len -= mps;
        else
            len = 0;
    }
    return cnt;
}

void usb_set_device_address(uint32_t addr) {
    // called by usb_enumerate after set address
    g_devaddr = addr;
}

void usb_set_ep0_mps(uint32_t mps) {
    // EP0 max packet size depends on device descriptor byte 7
    g_ep0_mps = mps;
}

void usb_set_speed(uint32_t speed) {
    // speed is derived from HPRT bits in dwc2
    g_speed = speed;
}

void usb_set_bulk_endpoints(uint32_t in_ep, uint32_t out_ep, uint32_t mps) {
    // called after we parse the configuration descriptor
    g_bulk_in_ep = in_ep;
    g_bulk_out_ep = out_ep;
    g_bulk_mps = mps ? mps : 64;
    g_bulk_pid_in = PID_DATA0;
    g_bulk_pid_out = PID_DATA0;
}

static int ctrl_out_stage(const void* buf, uint32_t len, uint32_t pid) {
    // OUT stage: copy data into DMA buffer, then kick channel 0
    if (len > USB_DMA_BUF_SIZE)
        return -1;
    if (len)
        memcpy(dma_buf, buf, len);

    // control OUT uses channel 0
    uint32_t pktcnt = pktcnt_for_len(len, g_ep0_mps);
    return dwc2_channel_xfer(0, g_devaddr, 0, EP_TYPE_CONTROL, 0,
                             g_ep0_mps, pid, len, pktcnt, g_speed,
                             dma_buf, 0);
}

static int ctrl_in_stage(void* buf, uint32_t len, uint32_t pid, uint32_t* xfered) {
    // IN stage: DMA into buffer, then copy back to caller
    if (len > USB_DMA_BUF_SIZE)
        return -1;

    // control IN uses channel 1
    uint32_t pktcnt = pktcnt_for_len(len, g_ep0_mps);
    int r = dwc2_channel_xfer(1, g_devaddr, 0, EP_TYPE_CONTROL, 1,
                              g_ep0_mps, pid, len, pktcnt, g_speed,
                              dma_buf, xfered);
    if (r == 0 && buf && len) {
        // only copy the bytes we actually got
        uint32_t n = xfered ? *xfered : len;
        if (n > len)
            n = len;
        memcpy(buf, dma_buf, n);
    }
    return r;
}

int usb_control_transfer(const struct usb_setup* setup, void *data) {
    // Setup stage is always OUT with PID_SETUP
    if (ctrl_out_stage(setup, sizeof(*setup), PID_SETUP) < 0)
        return -1;

    uint32_t len = setup->wLength;
    uint32_t xfered = 0;

    if (len) {
        // There is a data stage
        if (setup->bmRequestType & 0x80) {
            // Data IN
            if (ctrl_in_stage(data, len, PID_DATA1, &xfered) < 0)
                return -1;
            // Status OUT
            if (ctrl_out_stage(0, 0, PID_DATA1) < 0)
                return -1;
        } else {
            // Data OUT
            if (ctrl_out_stage(data, len, PID_DATA1) < 0)
                return -1;
            // Status IN
            if (ctrl_in_stage(0, 0, PID_DATA1, 0) < 0)
                return -1;
        }
    } else {
        // No data stage
        // status is IN for OUT requests, OUT for IN requests
        if (setup->bmRequestType & 0x80) {
            if (ctrl_out_stage(0, 0, PID_DATA1) < 0)
                return -1;
        } else {
            if (ctrl_in_stage(0, 0, PID_DATA1, 0) < 0)
                return -1;
        }
    }

    return (int)xfered;
}

int usb_bulk_in(void* buf, uint32_t len) {
    // bulk IN uses channel 2
    if (g_bulk_in_ep == 0 || len == 0)
        return -1;
    if (len > USB_DMA_BUF_SIZE)
        len = USB_DMA_BUF_SIZE;

    uint32_t pktcnt = pktcnt_for_len(len, g_bulk_mps);
    uint32_t num_bytes_transferred = 0;
    int r = dwc2_channel_xfer(2, g_devaddr, g_bulk_in_ep, EP_TYPE_BULK, 1,
                              g_bulk_mps, g_bulk_pid_in, len, pktcnt,
                              g_speed, dma_buf, &num_bytes_transferred);
    if (r < 0)
        return -1;

    if (num_bytes_transferred && buf)
        memcpy(buf, dma_buf, num_bytes_transferred);

    // toggle PID for next transfer
    g_bulk_pid_in = (g_bulk_pid_in == PID_DATA0) ? PID_DATA1 : PID_DATA0;
    return (int)num_bytes_transferred;
}

int usb_bulk_out(const void* buf, uint32_t len) {
    // bulk out uses channel 2 in this stack
    if (g_bulk_out_ep == 0 || len == 0)
        return -1;
    if (len > USB_DMA_BUF_SIZE)
        len = USB_DMA_BUF_SIZE;

    memcpy(dma_buf, buf, len);

    uint32_t pktcnt = pktcnt_for_len(len, g_bulk_mps);
    int r = dwc2_channel_xfer(2, g_devaddr, g_bulk_out_ep, EP_TYPE_BULK, 0, g_bulk_mps, g_bulk_pid_out, len, pktcnt, g_speed, dma_buf, 0);
    if (r < 0)
        return -1;

    // toggle PID for next transfer
    g_bulk_pid_out = (g_bulk_pid_out == PID_DATA0) ? PID_DATA1 : PID_DATA0;
    return (int)len;
}

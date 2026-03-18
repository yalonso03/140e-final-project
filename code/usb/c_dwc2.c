#include "c_dwc2.h"

// mailbox interface for powering usb
#define MBOX_BASE       0x2000B880
#define MBOX_READ       (MBOX_BASE + 0x00)
#define MBOX_STATUS     (MBOX_BASE + 0x18)
#define MBOX_WRITE      (MBOX_BASE + 0x20)
#define MBOX_EMPTY      (1u << 30)
#define MBOX_FULL       (1u << 31)
#define MBOX_CH_PROP    8
#define GPU_MEM_OFFSET  0x40000000

#define TAG_SET_POWER_STATE 0x00028001
#define USB_DEVICE_ID       3
#define POWER_STATE_ON      (1u << 0)
#define POWER_STATE_WAIT    (1u << 1)

static inline uint32_t mbox_uncached(const volatile void *p) {
    // GPU uses a different view of RAM so we OR in the offset. this makes the address visible to the GPU property channel
    return (uint32_t)(uintptr_t)p | GPU_MEM_OFFSET;
}

static int mbox_call(volatile uint32_t* mbox) {
    // Fire a property channel request and wait for the response
    // mailbox is a tiny FIFO so we must poll for space
    dev_barrier();
    while (GET32(MBOX_STATUS) & MBOX_FULL){
    ;
    }
    PUT32(MBOX_WRITE, mbox_uncached(mbox) | MBOX_CH_PROP);
    dev_barrier();
    while (GET32(MBOX_STATUS) & MBOX_EMPTY){
;
    }
    uint32_t r = GET32(MBOX_READ);
    if ((r & 0xF) != MBOX_CH_PROP)
        return -1;
    if (mbox[1] != 0x80000000)
        return -1;
    return 0;
}

static void dwc2_power_on(void) {
    // Request USB power via the mailbox property interface
    // mailbox buffer format is size, request, tag, value buffer size, value length, values, end tag
    static volatile uint32_t mbox[8] __attribute__((aligned(16)));
    mbox[0] = 8 * 4;// size
    mbox[1] = 0; // request
    mbox[2] = TAG_SET_POWER_STATE;
    mbox[3] = 8;// value buffer size
    mbox[4] = 8; // value length
    mbox[5] = USB_DEVICE_ID;
    mbox[6] = POWER_STATE_ON | POWER_STATE_WAIT;
    mbox[7] = 0;// end tag
    mbox_call(mbox);
}

static inline uint32_t dwc2_read(uint32_t reg) {
    // simple wrapper so call sites look like register reads
    return GET32(reg);
}

static inline void dwc2_write(uint32_t reg, uint32_t v) {
    // simple wrapper so call sites look like register writes
    PUT32(reg, v);
}

void dwc2_init(void) {
    // Power + basic core reset/config for host mode
    dwc2_power_on();

    // Stop all activity and clear interrupts
    // this makes the core quiet while we reprogram it
    dwc2_write(DWC2_GAHBCFG, 0);
    dwc2_write(DWC2_GOTGCTL, 0);
    dwc2_write(DWC2_GINTMSK, 0);

    // Force host mode, UTMI+ 8-bit
    // clear conflicting bits before setting the mode bit
    uint32_t gusbcfg = dwc2_read(DWC2_GUSBCFG);
    gusbcfg &= ~((1u << 3) | (1u << 4) | (1u << 8) | (1u << 9) |
                 (1u << 17) | (1u << 19) | (1u << 20) | (1u << 22) | (1u << 30));
    gusbcfg |= (1u << 29);
    dwc2_write(DWC2_GUSBCFG, gusbcfg);

    delay_ms(20);

    // Stop PHY clock
    // controller will restart as needed
    dwc2_write(DWC2_ARM_USB_POWER, 0);

    // Host configuration: full-speed
    dwc2_write(DWC2_HCFG, 1);
    dwc2_write(DWC2_HFIR, 48000);

    // FIFO configuration
    // basic split between RX, non periodic TX, and periodic TX
    dwc2_write(DWC2_GRXFSIZ, 1024);
    dwc2_write(DWC2_GNPTXFSIZ, (1024 << 16) | 1024);
    dwc2_write(DWC2_HPTXFSIZ, (1024 << 16) | 1024);

    // Flush TX FIFO
    dwc2_write(DWC2_GRSTCTL, (0x10 << 6) | GRSTCTL_TXFFLSH);
    while (dwc2_read(DWC2_GRSTCTL) & GRSTCTL_TXFFLSH)
        ;
    // Flush RX FIFO
    dwc2_write(DWC2_GRSTCTL, GRSTCTL_RXFFLSH);
    while (dwc2_read(DWC2_GRSTCTL) & GRSTCTL_RXFFLSH)
        ;

    // Enable DMA and AHB
    // DMA makes transfers cheaper than PIO
    dwc2_write(DWC2_GAHBCFG, GAHBCFG_GLBLINTRMSK | GAHBCFG_DMAEN | GAHBCFG_HBSTLEN_INCR4);
    dwc2_write(DWC2_HAINTMSK, 0x7); // channels 0-2
    dwc2_write(DWC2_GINTMSK, (1u << 25) | (1u << 3) | (1u << 24));

    // Power on port
    dwc2_write(DWC2_HPRT, HPRT_PRTPWR);
}

void dwc2_wait_for_device(void) {
    // Busy wait until something connects
    while ((dwc2_read(DWC2_HPRT) & HPRT_CONNSTATUS) == 0){
        ;
    }
}

void dwc2_port_reset(void) {
    // Toggle reset bit
    // timing is per the DWC2 host spec
    uint32_t hprt = dwc2_read(DWC2_HPRT);
    hprt &= ~((1u << 1) | (1u << 2) | (1u << 3) | (1u << 5));
    hprt |= HPRT_PRTRESET;
    hprt |= HPRT_PRTPWR;
    dwc2_write(DWC2_HPRT, hprt);
    delay_ms(60);

    hprt &= ~HPRT_PRTRESET;
    hprt &= ~((1u << 1) | (1u << 2) | (1u << 3) | (1u << 5));
    hprt |= HPRT_PRTPWR;
    dwc2_write(DWC2_HPRT, hprt);
    delay_ms(60);
}

uint32_t dwc2_get_port_speed(void) {
    // HPRT bits 17:18 encode HS, FS, LS
    return (dwc2_read(DWC2_HPRT) >> HPRT_PORTSPD_SHIFT) & 0x3;
}

void dwc2_channel_halt(uint32_t ch) {
    // Stop a channel cleanly so we can reprogram it
    uint32_t hcint = dwc2_read(DWC2_HCINT(ch));
    if (hcint & HCINT_CHHLTD)
        dwc2_write(DWC2_HCINT(ch), HCINT_CHHLTD);

    uint32_t hcchar = dwc2_read(DWC2_HCCHAR(ch));
    hcchar |= HCCHAR_CHDIS | HCCHAR_CHENA;
    dwc2_write(DWC2_HCCHAR(ch), hcchar);

    uint32_t timeout = 1000000;
    while ((dwc2_read(DWC2_HCINT(ch)) & HCINT_CHHLTD) == 0) {
        if (--timeout == 0)
            break;
    }
    dwc2_write(DWC2_HCINT(ch), HCINT_CHHLTD);

    timeout = 1000000;
    while (dwc2_read(DWC2_HCCHAR(ch)) & HCCHAR_CHENA) {
        if (--timeout == 0)
            break;
    }

    dsb();
}

int dwc2_channel_xfer(uint32_t ch,
                      uint32_t devaddr,
                      uint32_t ep,
                      uint32_t eptype,
                      uint32_t dir_in,
                      uint32_t mps,
                      uint32_t pid,
                      uint32_t xfersize,
                      uint32_t pktcnt,
                      uint32_t speed,
                      void *dma_addr,
                      uint32_t *xfered) {
    // hardware expects pktcnt at least 1
    if (pktcnt == 0)
        pktcnt = 1;

    // Halt channel before reprogramming
    dwc2_channel_halt(ch);

    dwc2_write(DWC2_HCINT(ch), 0xFFFFFFFF);
    dwc2_write(DWC2_HCINTMSK(ch), 0x7FF);
    dwc2_write(DWC2_HCSPLT(ch), 0);

    // transfer size is 19 bits in HCTSIZ
    uint32_t hctsiz = (xfersize & 0x7FFFF);
    hctsiz |= (pktcnt & 0x3FF) << 19;
    hctsiz |= (pid & 0x3) << 29;
    dwc2_write(DWC2_HCTSIZ(ch), hctsiz);

    // program DMA address for this transfer
    dwc2_write(DWC2_HCDMA(ch), (uint32_t)(uintptr_t)dma_addr);

    uint32_t hcchar = 0;
    // Build up HCCHAR: address, endpoint, type, mps, direction, speed
    hcchar |= (devaddr & 0x7F) << HCCHAR_DEVADDR_SHIFT;
    hcchar |= (ep & 0xF) << HCCHAR_EPNUM_SHIFT;
    hcchar |= (eptype & 0x3) << HCCHAR_EPTYPE_SHIFT;
    hcchar |= (1u << HCCHAR_MC_SHIFT);
    hcchar |= (mps & HCCHAR_MPS_MASK);
    if (dir_in)
        hcchar |= HCCHAR_EPDIR_IN;
    if (speed == DWC2_SPEED_LS)
        hcchar |= HCCHAR_LSDEV;
    // odd or even frame scheduling for split transactions
    if (dwc2_read(DWC2_HFNUM) & 1)
        hcchar |= HCCHAR_ODDFRM;
    hcchar |= HCCHAR_CHENA;

    // push config into hardware
    dsb();
    dwc2_write(DWC2_HCCHAR(ch), hcchar);
    dsb();

    uint32_t hcint;
    uint32_t timeout = 1000000;
    do {
        hcint = dwc2_read(DWC2_HCINT(ch));
        if (hcint & HCINT_CHHLTD)
            break;
    } while (--timeout);

    // if the channel never halted, bail and clean up
    if (timeout == 0) {
        dwc2_channel_halt(ch);
        return -1;
    }

    // Ack interrupt status so future transfers are clean
    dwc2_write(DWC2_HCINT(ch), hcint);

    if (xfered) {
        // HCTSIZ tracks what is left to send
        uint32_t left = dwc2_read(DWC2_HCTSIZ(ch)) & 0x7FFFF;
        *xfered = xfersize - left;
    }

    // success conditions
    if (hcint & (HCINT_XFERCOMP | HCINT_ACK))
        return 0;

    // error conditions
    if (hcint & (HCINT_STALL | HCINT_NAK | HCINT_XACTERR | HCINT_AHBERR |
                 HCINT_BBLERR | HCINT_FRMOVRUN | HCINT_DATATGLERR))
        return -1;

    return -1;
}

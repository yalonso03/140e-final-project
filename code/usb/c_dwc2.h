#ifndef DWC2_H
#define DWC2_H

#include "rpi.h"

// BCM2835 DWC2 base
// this is the Synopsys USB core mapped into the Pi address space
#define DWC2_BASE           0x20980000

// Global registers (DWC2 core)
// used for global config, FIFO sizes, and interrupts
#define DWC2_GOTGCTL        (DWC2_BASE + 0x000)
#define DWC2_GAHBCFG        (DWC2_BASE + 0x008)
#define DWC2_GUSBCFG        (DWC2_BASE + 0x00C)
#define DWC2_GRSTCTL        (DWC2_BASE + 0x010)
#define DWC2_GINTSTS        (DWC2_BASE + 0x014)
#define DWC2_GINTMSK        (DWC2_BASE + 0x018)
#define DWC2_GRXFSIZ        (DWC2_BASE + 0x024)
#define DWC2_GNPTXFSIZ      (DWC2_BASE + 0x028)

// Host registers (host mode)
// HCFG and HPRT are the main ones we touch
#define DWC2_HCFG           (DWC2_BASE + 0x400)
#define DWC2_HFIR           (DWC2_BASE + 0x404)
#define DWC2_HFNUM          (DWC2_BASE + 0x408)
#define DWC2_HPTXFSIZ       (DWC2_BASE + 0x100)
#define DWC2_HAINTMSK       (DWC2_BASE + 0x418)
#define DWC2_HPRT           (DWC2_BASE + 0x440)

// Host channel registers (one block per channel)
// each channel has its own HCCHAR, HCTSIZ, HCDMA, etc
#define DWC2_HCCHAR(n)      (DWC2_BASE + 0x500 + (n) * 0x20)
#define DWC2_HCSPLT(n)      (DWC2_BASE + 0x504 + (n) * 0x20)
#define DWC2_HCINT(n)       (DWC2_BASE + 0x508 + (n) * 0x20)
#define DWC2_HCINTMSK(n)    (DWC2_BASE + 0x50C + (n) * 0x20)
#define DWC2_HCTSIZ(n)      (DWC2_BASE + 0x510 + (n) * 0x20)
#define DWC2_HCDMA(n)       (DWC2_BASE + 0x514 + (n) * 0x20)

// Power register (ARM-side wrapper)
// used to start and stop the USB PHY clock
#define DWC2_ARM_USB_POWER  (DWC2_BASE + 0xE00)

// Bit fields (only the ones we touch)
// keep this list small so it is obvious what code relies on
#define GAHBCFG_GLBLINTRMSK (1u << 0)
#define GAHBCFG_HBSTLEN_INCR4 (1u << 3)
#define GAHBCFG_DMAEN       (1u << 5)

#define GRSTCTL_RXFFLSH     (1u << 4)
#define GRSTCTL_TXFFLSH     (1u << 5)

#define HCCHAR_CHENA        (1u << 31)
#define HCCHAR_CHDIS        (1u << 30)
#define HCCHAR_ODDFRM       (1u << 29)
#define HCCHAR_DEVADDR_SHIFT 22
#define HCCHAR_EPTYPE_SHIFT 18
#define HCCHAR_LSDEV        (1u << 17)
#define HCCHAR_EPDIR_IN     (1u << 15)
#define HCCHAR_EPNUM_SHIFT  11
#define HCCHAR_MPS_MASK     0x7FF
#define HCCHAR_MC_SHIFT     20

#define HCINT_XFERCOMP      (1u << 0)
#define HCINT_CHHLTD        (1u << 1)
#define HCINT_STALL         (1u << 3)
#define HCINT_NAK           (1u << 4)
#define HCINT_ACK           (1u << 5)
#define HCINT_XACTERR       (1u << 7)
#define HCINT_BBLERR        (1u << 8)
#define HCINT_FRMOVRUN      (1u << 9)
#define HCINT_DATATGLERR    (1u << 10)
#define HCINT_AHBERR        (1u << 2)

#define HPRT_CONNSTATUS     (1u << 0)
#define HPRT_PRTRESET       (1u << 8)
#define HPRT_PRTPWR         (1u << 12)
#define HPRT_PORTSPD_SHIFT  17

// Endpoint types (USB spec)
#define EP_TYPE_CONTROL 0
#define EP_TYPE_ISO     1
#define EP_TYPE_BULK    2
#define EP_TYPE_INTR    3

// PIDs (DWC2 encoding)
#define PID_DATA0   0
#define PID_DATA2   1
#define PID_DATA1   2
#define PID_SETUP   3

// Port speeds (HPRT bits 17:18)
#define DWC2_SPEED_HS 0
#define DWC2_SPEED_FS 1
#define DWC2_SPEED_LS 2

void dwc2_init(void);
void dwc2_wait_for_device(void);
void dwc2_port_reset(void);
uint32_t dwc2_get_port_speed(void);

// Halt or restart a host channel
// needed because HCCHAR is not safe to reprogram while active
void dwc2_channel_halt(uint32_t ch);
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
                      void* dma_addr,
                      uint32_t* xfered);

#endif

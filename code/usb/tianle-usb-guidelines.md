# A mini HID driver
We are dealing with the DesignWare DWC2 (OTG) USB controller. Official documentation not available. Most of my work is a replication of CSUD and usbpi (see refs).

## 1. USB controller
First step is to get us able to talk to the DWC2 controller, which involves the following steps:

### a. Powering it on
To do this, we need to send a mailbox message to our good old mailbox interface at 0xB880 MMIO offset (more details in mbox writeups), that has this one specific property tag:

- tag: RPI_FIRMWARE_SET_POWER_STATE - 0x00028001
- total size: 8
- req size: 8
    - req data 1: 3 (USB_DEVICE_ID)
    - req data 2: 3 (POWER_STATE_ON (1 << 0)| POWER_STATE_WAIT (1 << 1))

A successful response should have mbox[1] == 0x80000000

### b. Make it into host mode
Here we need to configure a few registers, and to do so easier, we use `writel` and `readl` which is `PUT32` and `GET32` but wrapped with `dmb` and `dsb`. More details [here](https://github.com/rsta2/uspi/blob/80146e0c434d6a2111924af31dff3cfccdded20e/include/uspi/dwhci.h)

- GAHBCFG (0x20980008) -> 0 - stop all activity
- GOTGCTL (0x20980000) -> 0 - clear OTG ctrl
- GINTMSK (0x20980018) -> 0 - mask all ints
- GUSBCFG (0x2098000C) -> clear out bits `3,4,8,9,17,19,20,22,30` set bit 29
    - 3 = PHYIF = 0 (8-bit UTMI)
    - 4 = ULPI_UTMI_SEL = 0 (UTMI+)
    - 8 = SRP capable
    - 9 = HNP capable
    - 17 = ULPI_FSLS
    - 19 = ULPI_CLK_SUS_M
    - 20 = ULPI_EXT_VBUS_DRV
    - 22 = TERM_SEL_DL_PULSE
    - 29 = FORCE_HOST_MODE
    - 30 = FORCE_DEVICE_MODE

Then probably you will need a delay here, like ~20ms or so to give DWC2 time to adjust mode (if was not in host mode before). Condition check: `GINTSTS (0x20980014)` should has `bit 0 = 1` to indicate that it is in host mode.

### c. Initialize in host mode
Now we need to do some basic host-side USB initialization for our controller to talk in a normal way, which is another batch of reg settings. Reference [here](https://github.com/rsta2/uspi/blob/80146e0c434d6a2111924af31dff3cfccdded20e/lib/dwhcidevice.c#L427C9-L427C28)

Before everything, first stop the PHY clock:
- ARM_USB_POWER (0x20980E00) -> 0

Then set clock speed for FS/LS USB
- HCFG (0x20980400) -> 1
- HFIR (0x20980404) -> 48000

Configure FIFO (for these confusing names, refer to [some of the rust naming](https://docs.rs/bcm2835-lpa/latest/bcm2835_lpa/usb_otg_global/index.html) of these stuff)
- GRXFSIZ (0x20980024) -> 1024
- GNPTXFSIZ (0x20980028) -> (1024 << 16) | 1024
- HPTXFSIZ (0x20980100) -> (1024 << 16) | 1024

Flush both TX and RX FIFOs
- GRSTCTL (0x20980010) -> ((FIFONUM = 0x10) << 6) | 1 << 5 (GRSTCTL_TXFFLSH)
- wait until GRSTCTL (0x20980010) | 1 << 5 (GRSTCTL_TXFFLSH) becomes 0
- GRSTCTL (0x20980010) -> 1 << 4 (GRSTCTL_TXFFLSH)
- wait until GRSTCTL (0x20980010) | 1 << 4 (GRSTCTL_TXFFLSH) becomes 0

Enable DMA mode (with fix burst length) (for these confusing constants, the [andriod doc](https://android.googlesource.com/kernel/mediatek/+/android-6.0.0_r0.6/drivers/usb/gadget/s3c-hsotg.h) may be helpful)
- GAHBCFG (0x20980008) -> GAHBCFG_GLBLINTRMSK (1) | GAHBCFG_DMAEN (1 << 5) | GAHBCFG_HBSTLEN_INCR4 (1 << 3)
- HAINTMSK (0x20980418) -> 0b111 (enable interrups for channel 0,1,2)
- GINTMSK (0x20980018) -> (1u << 25) | (1u << 3) | (1u << 24)  (HCINT, SOF, PORT. **be careful with this since it may just blow up your IRQ**)

Lastly, power on the USB port:
- HPRT (0x20980440) -> (1 << 12) (do not RMW)

Then you can get the speed of this USB device (save for later use):
- spd = (HPRT (0x20980440) >> 17) & 3, 0=HS, 1=FS, 2=LS

Now we should have a initialized USB host ready to use (hopefully). Since this is a minimum doc, we don't do any condition check here (BAD), for doing it properly, refer to either CSUD or USBPI

## 2. Talking to the USB controller
Wait, we already did this right? We set a bunch of regs in MMIO just like we did for UART, but the real communication uses DMA (the DMA of the DWC2 controller).

The DMA communication between the ARM and the DWC2 is through different channels, and each channel can be connected with different USB devices with different purposes (the `EP_TYPE` stuff decides this).
So, it will always be good for us to keep track of what channels we assign for each device and what type of a channel they are. In this example, we will be using channels (0,1,2) for `CTRL_OUT`, `CTRL_IN`, and `INT`.
Each channel has its own control registers, which are:
```
- HCCHAR(n)   = (0x20980400 + 0x100 + (n) * 0x20) // Host Channel Characteristics
- HCSPLT(n)   = (0x20980400 + 0x104 + (n) * 0x20) // Host Channel Split Control
- HCINT(n)    = (0x20980400 + 0x108 + (n) * 0x20) // Host Channel Interrupt Status
- HCINTMSK(n) = (0x20980400 + 0x10C + (n) * 0x20) // Host Channel Interrupt Mask
- HCTSIZ(n)   = (0x20980400 + 0x110 + (n) * 0x20) // Host Channel Transfer Size / PID (packet ID not process ID)
- HCDMA(n)    = (0x20980400 + 0x114 + (n) * 0x20) // Host Channel DMA Address
```

Now let's get started with the real data transfer, a good reference for this section is the [kernel driver](https://github.com/torvalds/linux/blob/162b42445b585cd89f45475848845db353539605/drivers/usb/dwc2/hcd_ddma.c#L817)

First, we will need some buffer for the DMA to read/write, but caching may be a big problem here! Choose the address properly and do corresponding cache invalidation before interacting with DMA
to make sure the CPU and the GPU are seeing the same stuff. For this step, we just assume we have a large enought buffer named `dma_buf` and L2 coherent.

To send stuff to the DMA, first load your data into this `dma_buf` buffer (obviously), then a few other parameters to prepare:
- `n` - Host channel index
- `devaddr` - USB device address (0â€“127)
- `ep` - Endpoint number (0â€“15)
- `eptype` - Endpoint type
- `dir` - Transfer direction
- `mps` - Max Packet Size
- `pid` - Data Packet ID (PID)
- `xfersize` - Transfer byte count
- `pktcnt` - Packet count

Then we start to mess around with the DMA channel(n):
- ACK previous halt if this channel has a previous halt:
  If `HCINT(n) & 0b10` (HCINT_CHHLTD) then write `0b10` back to `HCINT(n)`
- Then force halt the channel (the halt dance):
    - RMW the `HCCHAR(n)` reg to turn on bits 30 (HCCHAR_CHDIS) and 31 (HCCHAR_CHENA)
    - Wait for the channel to halt (wait for `HCINT(n) & 0b10` to become not zero)
    - ACK the halt by writing `0b10` back to `HCINT(n)`
    - Wait for the channel to be fully down (`HCCHAR(n) & HCCHAR_CHENA`, bit 31 of the reg becomes zero)
    - `dsb()` (optional but just in case)
- Program the transfer:
    - Fill `HCINT(n)` with `0xFFFFFFFF` (clear any clearable status)
    - Set `HCINTMSK(n)` to `0x7FF` (mask channel interrups)
    - Set `HCSPLT(n)` to 0 (no split)
    - Set `HCTSIZ(n)` to `i=xfersize & 0x7FFFF` and:
        - i |= `pid` << 29
        - i |= `pktcnt` << 19
    - Set `HCDMA(n)` to your buffer address (`dma_buf`)
- Enable the transfer channel: it's basically constructing a good `HCCHAR` for the transfer, which contains bits:
    - `devaddr` << 22 (HCCHAR_DEVADDR_SHIFT)
    - `ep` << 11      (HCCHAR_EPNUM_SHIFT)
    - `eptype` << 18  (HCCHAR_EPTYPE_SHIFT)
    - 1 << 20         (HCCHAR_MC_SHIFT)
    - `mps` & 0x7FF   (HCCHAR_MPS_MASK)
    - If `dir` is in, then set bit 15  (HCCHAR_EPDIR_IN)
    - If this is a low speed device, set bit 17 (HCCHAR_LSDEV)
    - If this is an oddframe, set bit 29 (HCCHAR_ODDFRM)
        - You can figure this out on the fly by reading bit 0 of `HFNUM (0x20980408)`
        - In another word, the bit 29 of HCCHAR_ODDFRM should be the same as the bit 0 of `HFNUM (0x20980408)`
    - You are done, write this value to the `HCCHAR(n)` register and the channel should be alive
    - Do a `dsb()` if you want

By now, the DMA should already be happily sending you transfer, and if you want to know if it has finished, do the following:
- Poll `HCINT(n)` for your channel `n`
- If the bit 1 (HCINT_CHHLTD) is set, then it has finished (but we don't know if its successful or not)
- Check for bit 0 (HCINT_XFERCOMP), which indicates a succesful transfer
- Sometime bit 5 (HCINT_ACK) also indicates a succesful transfer
- There are many other status you can check for (refer to the constants looking like `HCINTMSK_XFERCOMPL` in the [kernel driver](https://github.com/torvalds/linux/blob/162b42445b585cd89f45475848845db353539605/drivers/usb/dwc2/hw.h#L759))
    - HCINT_XFERCOMP
    - HCINT_STALL
    - HCINT_NAK
    - HCINT_FRMOVRUN
    - HCINT_DATATGLERR
    - HCINT_XACTERR
    - HCINT_AHBERR
    - HCINT_BBLERR
    - HCINT_ACK
- No matter what, you should write the value you got back to `HCINT(n)` to ACK.

After failed transaction, you should halt the host channel used to rearm the DMA correctly, which follows the same procedues as when we halt the channel at the beginning of the transaction.

## 3. USB protocol layer
Here we show a simple example of waiting for device to connect and interacting with it. More can be done here to detect the connect/disconnect of a device and handle properly. Ground truth details [here](https://www.usb.org/document-library/usb-20-specification).

### a. Waiting for connection and initialization
To determine if the port is connected with a device, simply check the bit 0 of `HPRT` (0x20980440), we can do this step by polling this register repeatedly.

Then, once this bit becomes 1, we will need to reset the USB port to initialize it (strange design but it will not talk until a port reset).
We again do so using the `HPRT` register:
- Fist unset bits 1,2,3,5 which are the status W1C bits for (connected change, enable change, overcurrent change)
- Set bit 8 (reset bit)
- Wait for around 10-20ms (usually), but longer is fine (mine uses 60)
- Unset bits 1,2,3,5,8 to clear the reset
- Wait for a bit (10-20ms, I use 60)
- Read the `HPRT` register again to get the speed of the device (bits 17,18)

After this, we can move on to talking to it using DMA

### b. Set device address
The default address of a new USB device is always 0, and random stuff happens if we just talk to it over there, so we need first move it somewhere else.
We will use a `usb_setup` struct for this, and this same struct will be used quite often for future steps:
[ref](https://github.com/rsta2/uspi/blob/80146e0c434d6a2111924af31dff3cfccdded20e/include/uspi/usb.h#L54)
```
struct usb_setup {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed, aligned(4)));
```
For the `SET_ADDRESS` request, we fill it like this, where `new_addr` is the new address that it should be on:
```
struct usb_setup setup = {
    .bmRequestType = 0x00,
    .bRequest = 5,          // SET_ADDRESS
    .wValue = new_addr,
    .wIndex = 0,
    .wLength = 0
};
```
After we have this data, we copy it to the `dma_buf` and start the channel to talk to it. Here are the additional params for starting the transaction:
- channel = 0
- devaddr = 0
- ep = 0
- eptype = EP_TYPE_CONTROL (0)
- dir = out
- mps = 8
- pid = PID_SETUP (3)
- xfersize = 8
- pktcnt = 1
- speed (updated) = whatever you got from 3.a

If transfer succeeded, you should see `0x23` as the output of the `HCINT(0)`.
In this case, a single HCINT_CHHLTD without ACK or XFERCOMP is considered a failure.

### c. Get device descriptor
Now after we assign the new address, we can go ahead and get the device descriptor for it.
For this step, we will be using this setup command:
```
struct usb_setup setup = {
    .bmRequestType = 0x80,
    .bRequest = 6,    // GET_DESCRIPTOR
    .wValue = 0x0100, // Device descriptor
    .wIndex = 0,
    .wLength = len};
```
and the output of this command will be a device descriptor struct that looks like:
```
struct usb_device_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed));
```
To do so, we will need two DMA invocations: send the setup commands, and read in the descriptors
The process of sending command is similar to previous step, but with new commands and new device address.

Then, we allocate a buffer for the descriptor (`desc`) and start the DMA again with following parameters:
- channel = 1      <- note difference
- devaddr = 1      <- or whatever you set in previous step
- ep = 0
- eptype = EP_TYPE_CONTROL (0)
- dir = in         <- note difference
- mps = 8
- pid = DATA_1 (2) <- note difference
- xfersize = 8
- pktcnt = 1
- speed (updated) = whatever you got from 3.a

This request will only give us 8 bytes of data, but that is enough: we want to know the `bMaxPacketSize0` field
of the device and it will be included!

After having that, we can now do a full pull of the descriptor again using the previous method, but with a bigger
`mps` and `xfersize`. If that is still not enough for the full buffer, try to do it in multiple packets or with
multiple DMA-in invocations.


### d. Get config descriptor
**Everything below are based on a trace of my keyboard interacting with my computer** so I actually don't know which
step is required or optional, nor I can justify how the values being transmitted in each step is selected.

We will also need to get config descriptor using the following command, the transaction should be submitted using the
same procedure as the previous step:
```
struct usb_setup setup = {
    .bmRequestType = 0x80,
    .bRequest = 6,    // GET_DESCRIPTOR
    .wValue = 0x0200, // Configuration descriptor
    .wIndex = 0,
    .wLength = len};
```
The return value of this command will follow these struct (yes there might be multiple returned structs):
```
struct usb_config_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} __attribute__((packed));

struct usb_interface_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed));

struct usb_endpoint_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed));
```

### e. Set config
Similar to how we set address, we set the config using this command
```
struct usb_setup setup = {
    .bmRequestType = 0x00,
    .bRequest = 9, // SET_CONFIGURATION
    .wValue = 1,
    .wIndex = 0,
    .wLength = 0};
```
But after sending this command, we will have to do a `status in` transfer (an IN transfer with no data).
We can invoke the channel using these parameters
- channel = 1
- devaddr = 1           <- or whatever you set in previous step
- ep = 0
- eptype = EP_TYPE_CONTROL (0)
- dir = in
- mps (updated) = whatever you got from 3.d
- pid = PID_DATA1 (2)
- xfersize = 0          <- note difference
- pktcnt = 1
- speed = whatever you got from 3.a


## 4. HID
TBD, more details: https://www.usb.org/sites/default/files/hid1_11.pdf. This part is simple, just like set config, you will need to set two things:
1. HID protocol (boot/report)
```
struct usb_setup setup = {
    .bmRequestType = 0x21,
    .bRequest = 0x0B,   // SET_PROTOCOL
    .wValue = protocol, // 0=boot, 1=report
    .wIndex = interface, // 0 is fine
    .wLength = 0};
```

2. Send an HID report (I don't know why but my keyboard requires this to turn on):
```
struct usb_setup setup = {
    .bmRequestType = 0x21, // Class, Interface, Out
    .bRequest = 0x09,      // SET_REPORT
    .wValue = (report_type << 8) | report_id, // 2 for type, 0 for id
    .wIndex = interface,   // 0 is fine
    .wLength = len};       // length of the data, here we send a single byte, set to 1
```
Then, do another DMA out call on the `CTRL_OUT` channel with `DATA_1` pid to send the byte (0x01)

That's it, the keyboard should light up.

## 5. Polling
TBD, but to really get USB data, you will have to poll the `INTERRUPT` channel with `EP_TYPE_INTERRUPT` type.
If ever that channel halts, it means there is one HID frame in the DMA buffer. You will need to rearm the DMA after it halts.
So the poll is really just checking channel state, which can be hooked up to an interrupt on IRQ (but I haven't tested that).
Sample poll transactions:
- channel = 2
- devaddr = 1           <- or whatever you set in previous step
- ep = (updated) whatever you got back from the get config command
- eptype = EP_TYPE_INTERRUPT (3)
- dir = in
- mps = whatever you got from 3.d
- pid = PID_DATA1 (2) or PID_DATA0 (0), depending on which one you used previously, the next one needs to be different
- xfersize = 8          <- note difference, HID report length
- pktcnt = 1
- speed = whatever you got from 3.a
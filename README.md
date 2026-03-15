# Final Project for CS140e: Breaking into Lyman
By: Jacob Roberts-Baca (jtrb) and Yasmine Alonso (yalonso)

Have you ever been locked out of your building at 3am without your ID and didn't want to have to call a friend to wake them up to have them let you in? This project is for you!

Most (all?) Stanford buildings allow people to call any resident from the front door via the landline that we have in our rooms. If the resident dialed picks up, and presses 9, the building unlocks. Our goal for the project was to automate this pickup/dial 9 process so that we always have the option to get into our building in the case of a lockout. You can find all the code in this repo + 

Take a look at a demo video of our project in action [here](TODO: insert youtube link to demo)!

## Usage
Getting the code (two options):
- Copy paste the `140e-final-proj` directory into your 140e repository (I kept it in my `/labs` folder)
OR
- Clone the repo and change your `CS140E_2026_PATH` env variable to point towards this repo. e.g. in .zshrc I added `export CS140E_2026_PATH="/Users/yasminealonso/cs140e_home/140e-final-proj/"`

Running the program:
- Run `make` in `code/`. This will produce `door_unlock_script.bin`.
- Then, simply run `pi-install app/door_unlock_script.bin`. 

Fun note: if you want to not have to hook this up to your laptop, you can provide power to the Pi via a power bank or some other source. Take the SD card out of your pi, and replace `kernel.img` with the door unlock binary. Rename the door unlock binary `kernel.img`. Now, when you put the SD card back in the pi, this door unlock binary will be the first thing to run as soon as the Pi is connected to power. So you can just leave the pi connected to the wall + the power bank and not have to sacrifice your laptop :-)

## Device Information
We purchased the [StarTech.com 56K USB Dial-up & Fax Modem](https://www.amazon.com/StarTech-com-56K-USB-Dial-up-Modem/dp/B01MYLE06I/ref=sr_1_3?adgrpid=186021838763&dib=eyJ2IjoiMSJ9.lOJFuKbl03QJ-b8_dLbA5NXidxTwK6leb7jUiJoYlaSd_dUE94DD7mpQcyeycpGeqLbOnP1CrJP1OzI3cjTgVVKRI1UwIhmBumshoiOpNvg2eirTV-xI_Fxb8i5DtLFVOjQk2EZz9HHD2Sv2hqipOnoGQ6KBCT34Gw_13Dq198yKg6vLdmqFuGmE8NA8GgFP88Z8QQiBFGoB-UbqWHNljAWATzQbbJciq0K4jWohBV4.MRYsZAyG5VYTQs6eqbouZAPLQpWQm2AI6dITGzoVOg0&dib_tag=se&hvadid=779581331944&hvdev=c&hvexpln=0&hvlocphy=9031915&hvnetw=g&hvocijid=6400171205119250179--&hvqmt=b&hvrand=6400171205119250179&hvtargid=kwd-325685945691&hydadcr=24139_13533938_2335427&keywords=startech+usb+fax+modem&mcid=f4f95e7e5b8b3dbbb3c93d3d9d484415&qid=1773169012&sr=8-3) off of Amazon which allows us to send/receive commands (pick up phone, dial 9, hang up phone, etc.) to masquerade our Pi as a telephone.

Simply plugging this device into a computer (laptop, Raspberry Pi) and then into the landline port in your room allows you to make and receive phone calls and communicate other relevant information via AT commands, a set of text instructions that can be used to control modems, cellular modules, and IoT devices! Read more about AT commands [here](https://onomondo.com/blog/at-commands-guide-for-iot-devices/) and also [on Wikipedia](https://en.wikipedia.org/wiki/Hayes_AT_command_set). We also explain a bit more about the relevant commands them below in our application section. 

Relevant documentation:
- [Modem datasheet](https://media.startech.com/cms/pdfs/usb56kemh2_datasheet.pdf?_gl=1*1wx31al*_gcl_au*Njg4MjI3MjI0LjE3NzMxNzExNzU.*_ga*MTc5NzI3ODUwLjE3NzMxNzExNzU.*_ga_YXX1RSJGSC*czE3NzMxNzExNzQkbzEkZzEkdDE3NzMxNzE1NjUkajYwJGwwJGgxOTY0MzgzMDgy). Doesn't have much information.
- [Conexant CX93010-2x Datasheet](https://datasheet.octopart.com/CX93010-22Z-Conexant-datasheet-180221437.pdf). This is the chip that powers the modem. Has helpful information about voice/fax classes.
- [Modem product listing](https://www.startech.com/en-us/networking-io/usb56kemh2?srsltid=AfmBOop4A9Q6g-aUonfiW6FAP7FHi1NitRrnLda2GFplWlGbtjo3gUif). The technical specifications portion of this made us feel decently confident pre-purchasing that our idea would work, as it says it supports DTMF (Dual Tone Multi Frequency, not the bad bunny song) signaling.

## Application 


## The USB stack
First, a massive thank you to Tianle Yu for his [documentation on building a USB Host Stack for DWC2](https://sites.tianleyu.com/~unics/cs140e/usb.md)! Without him, this would have not been possible!

The modem we use needs to communicate with the Pi over USB, which we do not currently have the support for. So, we needed to build a driver and a USB host stack for the DWC2 controller (hardware component inside the broadcom that allows us to send USB information).

==== Highest Level ====
- `app/door_unlock_script.c` implements the main application that can be run on the pi to constantly keep an eye out for calls to the landline + unlock the door.
- `drivers/cdc_acm.c` implements the driver that allows the Pi to treat the modem as a serial port. We use functions here in our application code to send the necessary AT commands.
    - Helpful documentation for this part:
        - [Universal Serial Bus Class Definitions for Communications Devices](https://www.usb.org/document-library/class-definitions-communication-devices-12). Download the zip linked here to get the PDF for errata/docs 
- `usb/usb_core.c` TODO. We use functions here in our application code to initialize 
- `usb/usb_transfer.c` implements bulk USB transfers. Used by read/write functionality in `cdc_acm.c`.
- `usb/dwc2.c` lowest level of the stack. Interacts with the DWC2 controller (hardware component inside of the Broadcom2835 chip on the Pi that allows us to send USB packets) via MMIO. 

==== Lowest Level ====
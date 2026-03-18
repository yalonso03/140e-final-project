# makefile copied from other labs 

# for running tests
PROGS := $(wildcard tests/[0-9]*-test.c)
# for when i finish door_unlock_scrpit.c
# PROGS := app/door_unlock_script.c

# usb stack and cdc driver
COMMON_SRC += usb/c_dwc2.c
COMMON_SRC += usb/b_usb_transfer.c
COMMON_SRC += usb/a2_usb_core.c
COMMON_SRC += usb/a1_cdc_acm.c

# relevant headers
INC += -I./usb

BOOTLOADER ?= pi-install
RUN ?= 0

include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v2

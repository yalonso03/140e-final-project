#include "rpi.h"
#include "usb/a2_usb_core.h"
#include "usb/a1_cdc_acm.h"

typedef enum {
    SerialEvent_Nothing = 0,
    SerialEvent_OK = 1,
    SerialEvent_Ring = 2,
    SerialEvent_Tone = 3,
    SerialEvent_Disconnect = 4,
} SerialEvent;

static const unsigned char OUT_OK[] = { '\r', '\n', 'O', 'K', '\r', '\n' };
static const unsigned char OUT_RING[] = { 0x10, 'R', '\r', '\n' };
static const unsigned char OUT_DTMF[] = { 0x10, '/' };
static const unsigned char OUT_DISCON[] = { 0x10, 'd' };
static const unsigned char OUT_BUSY[] = { 0x10, 'b' };

typedef struct {
    unsigned char buf[2048];
    unsigned buf_len;
    char tones[32];
    unsigned tones_len;
} AnsweringMachine;

static void am_consume_prefix(AnsweringMachine* am, unsigned n) {
    if (n == 0) return;
    if (n >= am->buf_len) {
        am->buf_len = 0;
        return;
    }
    for (unsigned i = 0; i + n < am->buf_len; i++) {
        am->buf[i] = am->buf[i + n];
    }
    am->buf_len -= n;
}

static void am_append_rx(AnsweringMachine* am, const unsigned char* data, unsigned n) {
    if (n == 0) return;
    if (n > sizeof(am->buf) - 1) {
        // Extremely unlikely: reset if the modem bursts too much data.
        am->buf_len = 0;
    }
    if (am->buf_len + n > sizeof(am->buf)) {
        // Avoid overflow; drop buffered prefix rather than crash.
        am->buf_len = 0;
    }
    unsigned copy_n = n;
    if (copy_n > sizeof(am->buf) - am->buf_len) {
        copy_n = sizeof(am->buf) - am->buf_len;
    }
    for (unsigned i = 0; i < copy_n; i++) {
        am->buf[am->buf_len++] = data[i];
    }
}

static SerialEvent am_event(AnsweringMachine* am) {
    unsigned char tmp[256];
    int n = cdc_read((char*)tmp, sizeof(tmp));
    if (n > 0) {
        printk("IN:\t");
        for (int i = 0; i < n; i++) printk("%c", tmp[i]);
        printk("\n");
        am_append_rx(am, tmp, (unsigned)n);
    }

    // Match prefixes in the same order as the Python version.
    if (am->buf_len >= sizeof(OUT_OK) && memcmp(am->buf, OUT_OK, sizeof(OUT_OK)) == 0) {
        am_consume_prefix(am, sizeof(OUT_OK));
        return SerialEvent_OK;
    }

    if (am->buf_len >= sizeof(OUT_RING) && memcmp(am->buf, OUT_RING, sizeof(OUT_RING)) == 0) {
        am_consume_prefix(am, sizeof(OUT_RING));
        return SerialEvent_Ring;
    }

    if (am->buf_len >= 6 && memcmp(am->buf, OUT_DTMF, sizeof(OUT_DTMF)) == 0) {
        // Python: self.tones += chr(self.buffer[3]); self.buffer = self.buffer[6:]
        unsigned char digit = am->buf[3];
        if (am->tones_len + 1 < sizeof(am->tones)) {
            am->tones[am->tones_len++] = (char)digit;
            am->tones[am->tones_len] = 0;
        }
        am_consume_prefix(am, 6);
        return SerialEvent_Tone;
    }

    if (am->buf_len >= sizeof(OUT_DISCON) && memcmp(am->buf, OUT_DISCON, sizeof(OUT_DISCON)) == 0) {
        am_consume_prefix(am, sizeof(OUT_DISCON));
        return SerialEvent_Disconnect;
    }

    if (am->buf_len >= sizeof(OUT_BUSY) && memcmp(am->buf, OUT_BUSY, sizeof(OUT_BUSY)) == 0) {
        am_consume_prefix(am, sizeof(OUT_BUSY));
        return SerialEvent_Disconnect;
    }

    // Otherwise, consume up to and including the first '\r'.
    int idx = -1;
    for (unsigned i = 0; i < am->buf_len; i++) {
        if (am->buf[i] == '\r') {
            idx = (int)i;
            break;
        }
    }
    if (idx >= 0) {
        am_consume_prefix(am, (unsigned)idx + 1);
    }

    return SerialEvent_Nothing;
}

static void am_wait_for(AnsweringMachine* am, SerialEvent target) {
    while (am_event(am) != target) {
        // Busy-wait, but avoid a hot spin.
        delay_ms(1);
    }
}

static void am_send_command(AnsweringMachine* am, const char* cmd) {
    printk("OUT:\t%s\n", cmd);
    cdc_write(cmd, (unsigned)strlen(cmd));
    cdc_write("\r", 1);
}

static void am_send_command_and_wait_ok(AnsweringMachine* am, const char* cmd) {
    am_send_command(am, cmd);
    am_wait_for(am, SerialEvent_OK);
}

static void play_brandenburg(AnsweringMachine* am) {
    // Same command list as `code/python/script.py`.
    static const char* commands[] = {
        "AT+VTS=[784,784,20]",
        "AT+VTS=[739,739,20]",
        "AT+VTS=[784,784,40]",
        "AT+VTS=[587,587,20]",
        "AT+VTS=[523,523,20]",
        "AT+VTS=[587,587,40]",
        "AT+VTS=[784,784,20]",
        "AT+VTS=[739,739,20]",
        "AT+VTS=[784,784,40]",
        "AT+VTS=[493,493,20]",
        "AT+VTS=[440,440,20]",
        "AT+VTS=[493,493,40]",
        "AT+VTS=[784,784,20]",
        "AT+VTS=[739,739,20]",
        "AT+VTS=[784,784,40]",
        "AT+VTS=[392,392,20]",
        "AT+VTS=[440,440,20]",
        "AT+VTS=[493,493,40]",
        "AT+VTS=[554,554,40]",
        "AT+VTS=[587,587,20]",
        "AT+VTS=[554,554,20]",
        "AT+VTS=[587,587,20]",
        "AT+VTS=[659,659,20]",
        "AT+VTS=[587,587,20]",
        "AT+VTS=[739,739,20]",
        "AT+VTS=[587,587,20]",
        "AT+VTS=[784,784,20]",
    };

    for (unsigned i = 0; i < (unsigned)(sizeof(commands) / sizeof(commands[0])); i++) {
        am_send_command_and_wait_ok(am, commands[i]);
    }
}

void notmain(void) {
    uart_init();  // so we can print debug to console
    printk("STARTING DOOR UNLOCK SCRIPT\n");

    usb_init();
    if (usb_enumerate() < 0) {
        panic("usb enumerate failed :(");
    }

    if (cdc_init() < 0) {
        panic("cdc init failed :((((");
    }

    printk("CDC all set!!!\n");

    AnsweringMachine am;
    am.buf_len = 0;
    am.tones_len = 0;
    am.tones[0] = 0;

    while (1) {
        // Reset state at the top of each run (matches Python).
        am.tones_len = 0;
        am.tones[0] = 0;
        int disconnected = 0;

        am_send_command_and_wait_ok(&am, "AT&F");
        am_send_command_and_wait_ok(&am, "ATE0");
        am_send_command_and_wait_ok(&am, "ATH");
        am_send_command_and_wait_ok(&am, "ATS0=1");
        am_send_command_and_wait_ok(&am, "AT+FCLASS=8");
        am_send_command_and_wait_ok(&am, "AT+VLS=0");

        am_wait_for(&am, SerialEvent_Ring);
        am_wait_for(&am, SerialEvent_OK);

        play_brandenburg(&am);

        while (am.tones_len < 4) {
            SerialEvent evt = am_event(&am);
            if (evt == SerialEvent_Disconnect) {
                disconnected = 1;
                break;
            }
        }

        if (!disconnected && strcmp(am.tones, "1824") == 0) {
            am_send_command_and_wait_ok(&am, "AT+VTS=9");
            printk("YAY:\tDoor Unlocked!\n");
        }

        am_send_command_and_wait_ok(&am, "ATH");
    }
}

# Python version of app/door_unlock_script.c, potentially easier to understand
from enum import Enum
import serial

class SerialEvent(Enum):
    Nothing = 0
    OK = 1
    Ring = 2
    Tone = 3
    Disconnect = 4

OUT_OK = b'\r\nOK\r\n'
OUT_RING = b'\x10R\r\n'
OUT_DISCON = b'\x10d'
OUT_BUSY = b'\x10b'
OUT_DTMF = b'\x10/'

class AnsweringMachine:

    def __init__(self, serial: serial.Serial):
        self.serial = serial
        self.buffer = b''
        self.tones = ''

    def event(self) -> SerialEvent:
        data = self.serial.read(1024)
        if data:
            print(f"IN:\t{repr(data.decode())[1:-1]}")

        self.buffer += data

        if self.buffer.startswith(OUT_OK):
            self.buffer = self.buffer[len(OUT_OK):]
            return SerialEvent.OK

        if self.buffer.startswith(OUT_RING):
            self.buffer = self.buffer[len(OUT_RING):]
            return SerialEvent.Ring

        if self.buffer.startswith(OUT_DTMF) and len(self.buffer) >= 6:
            # Upon receiving a dial tone, the format the data comes in as is: 
            #   \0x10 / \0x10 [NUMBER] \0x10 ~ 
            # with the [NUMBER] being at the 3rd dex
            # need to ensure buffer length >= 6 because we want to ensure we've pulled the entire DTMF message
            self.tones += chr(self.buffer[3])
            self.buffer = self.buffer[6:]
            return SerialEvent.Tone

        if self.buffer.startswith(OUT_DISCON):
            self.buffer = self.buffer[len(OUT_DISCON):]
            return SerialEvent.Disconnect

        if self.buffer.startswith(OUT_BUSY):
            self.buffer = self.buffer[len(OUT_BUSY):]
            return SerialEvent.Disconnect

        # Otherwise, consume up to and including the first '\r'
        idx = self.buffer.find(b'\r')
        if idx >= 0:
            self.buffer = self.buffer[idx + 1:]

        return SerialEvent.Nothing

    def wait_for(self, evt: SerialEvent) -> None:
        while self.event() != evt:
            pass

    def send_command(self, cmd: str) -> None:
        print(f"OUT:\t{cmd}")
        self.serial.write(cmd.encode() + b'\r')
        self.wait_for(SerialEvent.OK)

    def play_brandenburg(self):
        commands = [
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
        ]

        for cmd in commands:
            self.send_command(cmd)

    def run(self) -> None:
        while True:
            # Reset the modem state
            self.tones = ""
            disconnected = False

            self.send_command("AT&F")
            self.send_command("ATE0")
            self.send_command("ATH")
            self.send_command("ATS0=1")
            self.send_command("AT+FCLASS=8")
            self.send_command("AT+VLS=0")

            self.wait_for(SerialEvent.Ring)
            self.wait_for(SerialEvent.OK)

            # Play a "Baroque" melody
            #self.send_command("AT+VTS=[523,659,100],[659,784,100],[784,1047,100]")

            # play a baroque (actually!) melody
            self.play_brandenburg()

            while len(self.tones) < 4:
                evt = self.event()
                if evt == SerialEvent.Disconnect:
                    disconnected = True
                    break
            
            if not disconnected and self.tones == "1824":
                self.send_command("AT+VTS=9")
                print("YAY:\tDoor Unlocked!")

            self.send_command("ATH")


PORT = "/dev/tty.usbmodem246802461"
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=0)
am = AnsweringMachine(ser)
am.run()

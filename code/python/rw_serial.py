# Sample script that allows you to read/write to serial via stdin/out -- good for testing the modem
import serial
import sys
import threading

PORT = "/dev/cu.usbmodem246802461"
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=0)

def read_serial():
    while True:
        data = ser.read(1024)
        if data:
            sys.stdout.write(data.decode(errors="ignore"))
            sys.stdout.flush()

def write_serial():
    while True:
        line = sys.stdin.readline()
        if not line:
            break
        ser.write((line.rstrip('\n') + '\r').encode())

threading.Thread(target=read_serial, daemon=True).start()
write_serial()



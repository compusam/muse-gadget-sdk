#!/usr/bin/env python3
"""Log the board's serial output to a file without resetting it.

Opening the port resets the Core2 (DTR/RTS), so open it ONCE and keep this
running; send commands meanwhile with: printf '>chat=hello\n' > /dev/ttyUSB0
The SDK token (mgst_...) is masked in the log.
Usage: serial_logger.py LOGFILE [PORT]
"""
import re, sys, time
import serial

s = serial.Serial()
s.port = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyUSB0"
s.baudrate = 115200
s.timeout = 1
s.dtr = False
s.rts = False
s.open()
f = open(sys.argv[1], "a", buffering=1)
buf = b""
while True:
    d = s.read(4096)
    if not d:
        continue
    buf += d
    while b"\n" in buf:
        line, buf = buf.split(b"\n", 1)
        t = re.sub(r"\x1b\[[0-9;]*m", "", line.decode("utf-8", "replace").rstrip())
        t = re.sub(r"mgst_[A-Za-z0-9_-]+", "mgst_****", t)
        f.write(time.strftime("%H:%M:%S ") + t + "\n")

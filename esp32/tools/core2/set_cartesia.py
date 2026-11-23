#!/usr/bin/env python3
"""Send the Cartesia API key (hidden input) and a voice id to the board over serial.

Run in a real terminal (needs a TTY). Usage: set_cartesia.py VOICE_ID [PORT]
The key is never printed; it is saved in the board's NVS (">cartesia.key=").
Opening the port resets the board; wait for it to reboot afterwards.
"""
import getpass, sys, time
import serial

if len(sys.argv) < 2:
    sys.exit("usage: set_cartesia.py VOICE_ID [PORT]")
voice = sys.argv[1]
port = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyUSB0"
key = getpass.getpass("Cartesia API key (sk_car_..., hidden): ").strip()
if not key.startswith("sk_"):
    sys.exit("That does not look like a Cartesia key (sk_car_...)")

s = serial.Serial(port, 115200, timeout=0.5)
time.sleep(4)   # let the reset after opening the port finish booting
s.reset_input_buffer()
for name, val in (("cartesia.voice", voice), ("cartesia.key", key)):
    s.write((">%s=%s\n" % (name, val)).encode())
    s.flush()
    time.sleep(1)
out = s.read(4096).decode("utf-8", "replace").replace(key, "sk_****")
print("\n".join(l for l in out.splitlines() if "cartesia" in l.lower()) or "no reply; check the log")

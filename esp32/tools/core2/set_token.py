#!/usr/bin/env python3
"""Send the Muse SDK token to the board over serial without echoing it. Run in a real terminal (needs a TTY). Usage: set_token.py [PORT]"""
import getpass, re, sys, time
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
token = getpass.getpass("SDK token (mgst_..., hidden): ").strip()
if not token.startswith("mgst_"):
    sys.exit("The token must start with mgst_")

def mask(text):
    return text.replace(token, "mgst_****")

s = serial.Serial(PORT, 115200, timeout=0.5)
s.reset_input_buffer()
s.write((">sdk_token=" + token + "\n").encode())
s.flush()

out, end, ok = "", time.time() + 8, False
while time.time() < end:
    d = s.read(4096)
    if d:
        out += d.decode("utf-8", "replace")
        if "@sdk_token ok" in out:
            ok = True
            break
print(mask(re.sub(r"\x1b\[[0-9;]*m", "", out)).strip()[-600:])
print("\nRESULT:", "token saved (@sdk_token ok)" if ok else "no confirmation; check the log")

# Let the board reboot and show the boot log (token masked).
if ok:
    time.sleep(1)
    end, boot = time.time() + 15, ""
    while time.time() < end:
        d = s.read(4096)
        if d:
            boot += d.decode("utf-8", "replace")
    boot = mask(re.sub(r"\x1b\[[0-9;]*m", "", boot))
    keep = [l for l in boot.splitlines()
            if re.search(r"token|pair|advertis|ble|wifi creds|error|E \(|panic", l, re.I)]
    print("\n".join(keep[:25]))

# M5Stack Core2 (v1.0) guide

Field notes from running this SDK on an **M5Stack Core2 for AWS IoT EduKit v1.0**
(AXP192 PMU, ESP32-D0WDQ6-V3, 16 MB flash, 8 MB PSRAM). The v1.1 (AXP2101) is not supported.

## Build and flash

```sh
git clone --recursive https://github.com/compusam/muse-gadget-sdk
cd muse-gadget-sdk/esp32
. ~/esp/esp-idf-v6/export.sh          # ESP-IDF v6.0.3 exactly (see esp32/README.md)
tools/muse/board.sh build core2
tools/muse/board.sh flash core2 /dev/ttyUSB0   # 230400 baud (CP2104 bridge)
```

Back up the stock flash first if you want to return to it:
`python -m esptool --chip esp32 -p /dev/ttyUSB0 -b 230400 read-flash 0 0x1000000 core2-backup.bin`.
Reflashing keeps pairing and Wi-Fi (NVS is untouched).

## SDK token and pairing

1. Get a token at <https://gadgets.muse.ai/settings/sdk-tokens>.
2. Send it without echoing it on screen (needs a real terminal, not an IDE `!` prompt):
   `python esp32/tools/core2/set_token.py /dev/ttyUSB0` (needs `pyserial`).
3. In the Muse app (Developer mode on) pair with `MuseGadget-XXXXXX`.
4. **Confirm by tapping the middle touch strip *below* the LCD** (the centre "B" circle,
   touch `y >= 240`). Tapping the screen does nothing for pairing.

## What this fork adds

| Feature | Where |
|---|---|
| Tap Muse on the screen: ascending chirp + 250 ms rainbow on the side bars + green LED flash | `muse_ui.c`, `board_m5stack_core2.c` |
| Side **RGB bars** (10 x SK6812 on GPIO25, powered by the AXP192 EXTEN 5 V rail) that follow Muse's state: idle teal breathing, listening green, thinking purple pulse, speaking cyan pulse, error red blink, off when asleep | `board_m5stack_core2.c` |
| Green power LED (AXP192 GPIO1): solid while the talk zone is held | `board_m5stack_core2.c` |
| Optional board hook `set_led(bool)` | `muse_board.h` |
| Fix: the chirp was cut off because the I2S0 channel switched to the mic before the DMA queue drained; 125 ms of trailing silence | `muse_voice.c` |
| `led_strip` component enabled for `m5stack_core2` | `idf_component.yml` |

The same changes are in [`mis-cambios.patch`](../../mis-cambios.patch) (apply to a clean
upstream checkout with `git apply mis-cambios.patch`).

Tunables: `RGB_MAX` (brightness, default 40/255) and `RGB_COUNT` in `board_m5stack_core2.c`.

## Known limits

- **Replies are text only.** This SDK shows Muse's answers as captions paced with silence;
  there is no text-to-speech. The speaker works (chirp, `m` test MP3), so a TTS hook could go in
  `muse_chat_session.cpp` (reply handling, ~line 2146). PRs welcome.
- Short voice notes (< ~2 s, near silence) get no useful reply: hold the strip and speak up.
- I2S0 is shared by the PDM mic (GPIO0/34) and the NS4168 speaker (GPIO12/0/2): one direction
  at a time.

## Debugging tips

- **Opening `/dev/ttyUSB0` resets the board.** Use `esp32/tools/core2/serial_logger.py LOG`
  once, in the background, and read the file; send commands with
  `printf '>chat=hello\n' > /dev/ttyUSB0`. Only one process may hold the port, so stop the
  logger before flashing or `idf.py monitor`.
- Serial commands: `>chat=TEXT`, `>volume=N`, `>test.loopback`, `w` (wake), `m` (test MP3).
- Healthy status line: `setup=done wifi=up ws=up raw=up ble=off`.
- `Wi-Fi reason=15` is a WPA2 handshake failure, usually a wrong password: re-send credentials
  from the app.

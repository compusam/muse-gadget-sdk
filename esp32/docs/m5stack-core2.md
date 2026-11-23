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
| **Spoken replies** with [Cartesia](https://cartesia.ai) TTS (Spanish, any language Sonic supports): reply text is sent to the API, the returned MP3 plays through the speaker; falls back to text captions on any failure | `muse_cartesia.c`, `muse_chat_session.cpp` |
| Fix: the chirp was cut off because the I2S0 channel switched to the mic before the DMA queue drained; 125 ms of trailing silence | `muse_voice.c` |
| `led_strip` component enabled for `m5stack_core2` | `idf_component.yml` |

The same changes are in [`mis-cambios.patch`](../../mis-cambios.patch) (apply to a clean
upstream checkout with `git apply mis-cambios.patch`).

Tunables: `RGB_MAX` (brightness, default 40/255) and `RGB_COUNT` in `board_m5stack_core2.c`.

## Spoken replies (Cartesia)

Muse's replies are text in the stock SDK. This fork can speak them with Cartesia (voice notes
only: typed or `>chat=` messages stay text, like upstream).

1. Create an API key at <https://play.cartesia.ai/keys> and pick a voice id in the
   [Voice Library](https://play.cartesia.ai/voices) (filter Spanish > Mexican for Latin American
   Spanish; "Copy ID" in the row menu). Example: Ximena, `3597a26f-80ef-4bd5-8101-9699bc764917`.
2. Save both on the board (key typed with hidden input, in a real terminal; needs `pyserial`):
   `python esp32/tools/core2/set_cartesia.py VOICE_ID /dev/ttyUSB0`
3. Check it: `printf '>cartesia.test=Hola\n' > /dev/ttyUSB0` logs `speech ready: N bytes of MP3`.
   Serial commands: `>cartesia.key=`, `>cartesia.voice=`, `>cartesia.test=TEXT`. The key is never logged.
4. Hold the strip and talk; the reply is spoken. Without a key the reply is shown as text as before.

Audio streams: playback starts after ~1 s of speech has arrived (about 2 s after the reply text is complete) instead of waiting for the whole MP3.
Model `sonic-3.6`, language `es` (change in `muse_cartesia.c`), MP3 24 kHz / 32 kbps, text capped
at 700 characters. Cartesia's free plan is 20K credits per month (about 1 per character) and has no
commercial licence. Your key lives only in the board's NVS.

## Known limits

- Replies are spoken only for voice notes; text turns stay silent captions.
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

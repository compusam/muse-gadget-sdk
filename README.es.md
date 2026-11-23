# Muse Gadgets: fork para M5Stack Core2

[English](README.md) · **Español**

> **Fork comunitario de [facebookincubator/muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk)**
> con funciones extra para el **M5Stack Core2 (v1.0)**. No está afiliado ni respaldado por Meta.
> El README original (en inglés) sigue en [`README.md`](README.md), debajo de "Upstream README".

## Qué añade este fork

- **Toca a Muse y respóndele con sonido y luz:** al tocar a Muse en la pantalla suena un chirp
  y se encienden las luces.
- **Barras RGB laterales:** los 10 LEDs SK6812 del Core2 (GPIO25) muestran el estado de Muse:
  reposo, escuchando, pensando, hablando, error y dormido.
- **LED verde** mientras mantienes pulsada la zona para hablar.
- **Arreglo del chirp:** el altavoz ya no se corta por el cambio de canal del micrófono.
- **Herramientas** en [`esp32/tools/core2`](esp32/tools/core2): introducir el SDK token con
  entrada oculta y registrar el serial sin reiniciar la placa.
- Una [guía del Core2](esp32/docs/m5stack-core2.md) (en inglés): compilar, flashear, trampas del
  emparejamiento y depuración.

## Inicio rápido

```sh
git clone --recursive https://github.com/compusam/muse-gadget-sdk
cd muse-gadget-sdk/esp32
. ~/esp/esp-idf-v6/export.sh                  # ESP-IDF v6.0.3 exacta
tools/muse/board.sh build core2
tools/muse/board.sh flash core2 /dev/ttyUSB0
python tools/core2/set_token.py /dev/ttyUSB0  # tu SDK token, entrada oculta
```

Después empareja `MuseGadget-XXXXXX` desde la app Muse (con el modo Developer activado) y
**confirma tocando la franja táctil central que está debajo de la pantalla**; tocar la pantalla
no sirve para eso.

Solo se soporta el Core2 **v1.0** (AXP192), no el v1.1 (AXP2101).

## Consejos

- **Abrir `/dev/ttyUSB0` reinicia la placa.** Para ver el log sin reiniciarla usa
  `esp32/tools/core2/serial_logger.py LOG` una sola vez, en segundo plano, y lee el archivo.
  Para enviar comandos: `printf '>chat=hola\n' > /dev/ttyUSB0`.
- Para hablar con Muse, mantén la franja y habla fuerte más de 2 s; las notas muy cortas no
  reciben respuesta.
- Estado sano en el log: `setup=done wifi=up ws=up raw=up ble=off`.
- `Wi-Fi reason=15` suele ser contraseña incorrecta: reenvía las credenciales desde la app.

## Estado y hoja de ruta

Funciona: compilar y flashear, emparejamiento, Wi-Fi, micrófono, interfaz, chirp y barras RGB.
Falta: respuestas habladas (Muse responde como texto; una conexión a un TTS es la idea abierta),
más sonidos y temas de luz. Se aceptan issues y pull requests. Los cambios también están en
[`mis-cambios.patch`](mis-cambios.patch) respecto al repositorio original.

Necesitas tu propio [SDK token](https://gadgets.muse.ai/settings/sdk-tokens) y debes aceptar los
[términos del SDK](https://gadgets.muse.ai/sdk-terms). Nunca subas tu token al repositorio. Como
dice el original, úsalo bajo tu propio riesgo: son posibles placas inservibles y brownouts.

## Licencia

Apache 2.0, igual que el proyecto original; ver [`LICENSE`](LICENSE) y la tabla de archivos de
terceros en [`README.md`](README.md).

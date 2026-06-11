# Flasheo del miniHSM y lectura del log (operativo)

Dos herramientas web. Requieren Chrome o Edge (Web Serial API).
Firefox y Safari NO lo soportan.

## 1. Flashear el firmware — esptool-js
URL: https://espressif.github.io/esptool-js/

Pasos:
1. Descargar `minihsm-merged.bin` del release MAS RECIENTE en GitHub
   (en Releases, NO en Artifacts). El nombre del .bin es siempre igual:
   BORRAR el viejo de Descargas antes de bajar el nuevo, o flasheas uno viejo.
2. Connect -> seleccionar el puerto serial del device.
3. Flash Address: **0x0** -> elegir `minihsm-merged.bin` -> Program.
4. Al terminar: **Disconnect** (libera el puerto serial).

## 2. Ver el log — Google Chrome Labs Serial Terminal
URL: https://googlechromelabs.github.io/serial-terminal/

Pasos:
1. Connect -> seleccionar el puerto.
2. Baud rate: **115200**.
3. Muestra el log de arranque del firmware (match, heartbeat, jobs, etc.).

## Tip importante: un solo sitio toma el puerto a la vez
NO se puede tener esptool-js y el serial-terminal conectados al mismo puerto
al mismo tiempo. Orden correcto:
1. Flashear en esptool-js -> **Disconnect**.
2. Recien entonces conectar en el serial-terminal para ver el log.
Si el terminal dice que el puerto esta ocupado, es que esptool-js u otra
pestaña aun lo tiene tomado.

## Release actual
firmware-v35 (`minihsm-merged.bin` @ 0x0).

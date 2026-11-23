# Sonda GoldHEN de solo lectura

Para compartir resultados de compatibilidad en otros firmwares/modelos, usa [la plantilla de reporte](PROBE_REPORT_TEMPLATE.md). Recompila el BIN con la IPv4 de tu PC; el binario del paquete viene configurado para la máquina original.

## Archivos y tamaño

- Fuente: [goldhen_readonly_probe.c](../probe/goldhen_readonly_probe.c)
- BIN raw: [goldhen_readonly_probe.bin](../probe/build/goldhen_readonly_probe.bin)
- Receptor: [receive_probe.py](../tools/receive_probe.py)
- Sender: [send_payload.py](../tools/send_payload.py)
- BIN recompilado: 110.736 bytes; SHA-256 `718f1d0d68dd5a2a12d5e7535121bb6ca0831d96ce55b401c6244103a656e61d`.
- La compilación actual muestra `kern.sdk_version` en hex y como entero little-endian (`0x09008031` para 9.00); no convierte esos cuatro bytes binarios en texto.
- El BIN entregado tiene incrustado el destino actual `<PS4_IP>:9090`; recompílalo si cambia la IPv4 del PC.

> **ADVERTENCIA CRÍTICA:** `--report-host` debe ser la IPv4 del PC receptor, nunca la IP de la PS4. Si se incrusta por error la IP de la consola, la sonda intentaría enviar el texto del informe al BinLoader, que puede interpretarlo como bytes de payload. Antes de cada envío confirma `output.tcp`/el destino de compilación y verifica que la IPv4 no sea la de la PS4.

El BIN previo medía 110.352 bytes. La mayor parte era espacio de datos reservado, no libc ni símbolos: 192 registros `statfs` de 472 bytes (90.624 bytes) y el informe (8.192 bytes). Ya se usan `-Os`, `-ffunction-sections` y `--gc-sections`; al ser salida raw de `objcopy`, `strip` no elimina esos buffers ni metadatos ELF. Los 384 bytes adicionales de esta compilación corresponden a la configuración del destino y al marcador explícito de fin.

## 1. Averiguar las IPv4

- **PC:** abre PowerShell y ejecuta `ipconfig`. Anota la dirección IPv4 del adaptador conectado a la misma LAN que la PS4 (no uses `0.0.0.0` como destino).
- **PS4:** `Settings > Network > View Connection Status`; anota la dirección IPv4.
- Asegura que ambos estén en la misma red local. El perfil actual de Windows es `Public`; el firewall descrito abajo se limita a la subred local.

## 2. Compilar con el destino del informe

Desde la raíz del proyecto, sustituye `<IP_PC>` por la IPv4 anotada:

```powershell
python tools/build_readonly_probe.py --report-host <IP_PC> --report-port 9090
python tools/audit_readonly_probe.py --report-host <IP_PC>
Get-FileHash probe/build/goldhen_readonly_probe.bin -Algorithm SHA256
```

`--report-host` es obligatorio; la IP no está fijada en el código fuente. El puerto de informe predeterminado es 9090 y puede cambiarse, pero debe coincidir con `--port` del receptor. La auditoría verifica que el BIN contiene el destino elegido y conserva las comprobaciones de syscalls/IOCTL prohibidos.

## 3. Regla de Firewall de Windows

Abre PowerShell **como administrador** y ejecuta esta regla acotada al ejecutable Python, TCP, el puerto elegido y la subred local. Para Python 3.12 y puerto 9090 en este PC:

```powershell
New-NetFirewallRule -DisplayName "PS4 Readonly Probe TCP 9090" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 9090 -Program "python.exe" -Profile Public -RemoteAddress LocalSubnet
```

Si Python está en otra ruta, consúltala con `(Get-Command python).Source` y reemplaza `-Program`. Si compilas con otro puerto, reemplaza el puerto tanto en la regla como en los comandos de recepción/build.

## 4. Iniciar receptor y cargar

En una ventana normal de PowerShell, desde la raíz del proyecto:

```powershell
python tools/receive_probe.py --host 0.0.0.0 --port 9090 --timeout 180
```

En PS4, carga GoldHEN y habilita el servicio en `Settings > ★ GoldHEN > Server Settings > Enable BinLoader Server` ([referencia de menú](https://consolemods.org/wiki/PS4%3AGoldHEN)). El BinLoader de GoldHEN escucha en el puerto 9090, según [GoldHEN](https://github.com/GoldHEN/GoldHEN/releases).

Envía el BIN desde otra ventana PowerShell:

```powershell
python tools/send_payload.py --ps4-ip <IP_PS4> --port 9090 --bin probe/build/goldhen_readonly_probe.bin
```

El sender informa que terminó de enviar bytes; eso **no prueba** que el payload finalizara. La confirmación es que el receptor imprima el reporte completo, incluido `probe.completed=yes` si pasó la precondición. No abras una conexión de prueba al puerto 9090: el BinLoader puede consumir su `accept` aunque cierres sin enviar datos. Envía directamente el payload permitido; si falla la conexión PC→PS4, revisa el ajuste BinLoader, la IP/puerto y la red. Si la sonda no conecta al PC, intenta guardar el reporte en `/mnt/usb0/ps4_readonly_probe.txt`, solo en USB FAT/exFAT verificado, sin sobrescribir un archivo existente. No se usa FTP/2121.

## Diagnóstico rápido

| Síntoma | Causa probable | Arreglo |
|---|---|---|
| No llega nada | IP PC equivocada en el BIN, red distinta o firewall bloquea | Revisa IPv4, recompila con `--report-host`, comprueba la regla y vuelve a iniciar el receptor antes del envío. |
| Conexión rechazada al enviar | BinLoader desactivado o puerto PS4 incorrecto | Activa `Enable BinLoader Server`; confirma `--ps4-ip` y `--port 9090`. |
| Timeout del receptor | No se envió la sonda o destino compilado incorrecto | Deja el receptor esperando, confirma `output.tcp` del BIN mediante el build y vuelve a enviar. |
| Bind fallido | Puerto ocupado, inválido o permiso/firewall local | Cierra el otro listener o usa el mismo puerto alternativo en build, receptor y regla de firewall. |
| Sender dice enviado, sin reporte | El BinLoader aceptó TCP pero el payload abortó, falló su precondición o no alcanzó la red de retorno | Revisa que cargaste el BIN recién compilado y que el PC acepte el puerto de informe; busca el archivo fallback en USB. |
| USB no montado / archivo ausente | No hay USB FAT/exFAT reconocido en `/mnt/usb0`, o el reporte anterior ya existe | Usa TCP; o conecta USB compatible y elimina/renombra desde PC el reporte antiguo antes de repetir. |

Si aparece `precondition=FAIL`, la sonda terminó limpiamente tras reportar UID/sandbox, pero no hizo las consultas restantes. Si aparece `probe.completed=yes` y el receptor muestra `Fin del reporte`, la sonda terminó su recorrido. Si la consola se queda sin responder, espera 60 segundos; si continúa congelada, mantén Power hasta el segundo beep para apagarla. No selecciones opciones de reinstalación/actualización. PlayStation documenta el segundo beep en su [guía de Safe Mode](https://www.playstation.com/en-us/support/hardware/safe-mode-playstation/).

## Qué consulta y qué no

Comprueba UID/GID/sandbox, firmware por dos APIs, `stat`/`open(O_RDONLY)` de `/dev/icc_fan`, dos GET `0xC01C8F08` con buffers cero de 64 bytes e informa 28 bytes de cada respuesta, y `stat` de las dos rutas `/update`. El GET se asume por WebRTE, no por prueba previa en esta PS4; si no fuera GET, el peor caso realista es aplicar al estado ICC una estructura inesperada. El buffer de 64 bytes evita lecturas/escrituras fuera de límites porque excede los 28 bytes codificados.

No hay ioctl SET, operación de escritura en HDD interno, ni rutas `mkdir/unlink/rmdir`. La escritura de fallback solo puede crear un archivo nuevo en USB externo verificado.

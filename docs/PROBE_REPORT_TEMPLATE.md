# Plantilla de reporte de compatibilidad de la sonda

Completa una copia por consola. Comparte el texto del informe y omite datos personales o de red que hayas añadido aparte. La sonda es de solo lectura para ICC y `/update`: no envía SET ni modifica las rutas del HDD interno.

> El BIN incluido en este proyecto devuelve su reporte a `<PS4_IP>:9090`. Antes de cargarlo, recompílalo con la IPv4 de tu PC mediante `python tools/build_readonly_probe.py --report-host <IP_PC> --report-port 9090` y audítalo con `python tools/audit_readonly_probe.py --report-host <IP_PC>`. Nunca uses la IP de la PS4 como destino del informe.

## Sistema

- Fecha/hora local:
- Modelo: FAT / Slim / Pro / desconocido:
- Firmware exacto informado por la sonda:
- GoldHEN exacto leído en el menú (si aplica):
- Loader y puerto de carga:
- ¿Recompilaste la sonda para la IPv4 del PC receptor?:

## Contexto y nodo ICC

- `identity.uid`:
- `identity.gid`:
- `identity.is_in_sandbox`:
- `precondition`:
- `/dev/icc_fan`: existencia, tipo, permisos y tamaño:
- `open` retorno y errno:

## GET ICC

| Lectura | Retorno | errno | 28 bytes completos |
|---|---:|---:|---|
| 1 | | | |
| 2 | | | |
- ¿Coinciden ambas lecturas?:

## Rutas `/update`

| Ruta | retorno stat / errno | tipo | permisos | tamaño |
|---|---|---|---|---|
| `/update/PS4UPDATE.PUP` | | | | |
| `/update/PS4UPDATE.PUP.net.temp` | | | | |

## Texto crudo

```text
Pega aquí la salida completa desde `probe=goldhen_readonly_probe` hasta `probe.completed=yes`.
```

La presencia del nodo o una lectura GET exitosa en una combinación no demuestra compatibilidad de otros firmwares, modelos o builds GoldHEN. No apliques un payload SET para completar este reporte.

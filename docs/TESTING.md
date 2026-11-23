# Protocolo y estado de las medidas GoldHEN

Las medidas se realizaron en PS4 Pro 9.00 + GoldHEN 2.4b18.10. `tools/run_measurement.py` verifica el SHA-256 allowlist antes de cada envío, abre el receptor TCP y conserva el informe/diff. No abras una conexión TCP vacía a 9090: puede consumir el `accept` de BinLoader. El usuario confirmó que las descargas automáticas del sistema están desactivadas, mantiene Internet y no cambió DNS.

**El runner y los comandos F0/F son específicos del workspace original y la PS4 del autor; no los ejecutes contra otra consola ni desde un paquete compartido.** Para comprobar compatibilidad en otra PS4 usa solamente la sonda recompilada con la IPv4 de tu PC y [la plantilla](PROBE_REPORT_TEMPLATE.md).

## Medidas fan

- A: estado inicial, byte 5=`0x4F`.
- B: fan60 original; cambió únicamente byte 5 `0x4F→0x3C`; el usuario confirmó notificación y aceleración audible.
- D: fan default original; volvió a byte 5=`0x4F`.
- E2: fan60 universal corregido, SHA-256 `7ee35577a903899037630ad5d201dc9a03aa480dbf9b757753186375ce702ad0`; validación GET/SET correcta, solo byte 5, sonda posterior en `0x3C`.
- C: sonda tras el reinicio reportado; detalles de secuencia en [C_AND_F_AUDIT.md](../analysis/C_AND_F_AUDIT.md).
- T50/T55/T65/T70/T75/T80/TDEFAULT: todos verificados; cada uno cambió únicamente byte 5 y la sonda confirmó la tabla. T80 emitió el aviso sobre superar el stock observado. Se terminó en 79. Ver [FAN_SMOKE_TESTS.md](../analysis/FAN_SMOKE_TESTS.md).

## C — persistencia tras reinicio

Antes del reinicio se aplicó E2 (`fan_threshold_60_universal.bin`, SHA arriba). El usuario informó que reinició y recargó GoldHEN. La invocación C envió solo la sonda, sin SET fan en esa ejecución. El archivo crudo [probe_C.txt](../analysis/probe_C.txt), guardado `2026-11-20 00:18:54 UTC`, muestra GET dos veces estable con byte 5=`0x3C`; ambas rutas `/update` eran directorios `0777`. Los smoke tests con fan payloads comenzaron después de C (el primero, T50, quedó guardado a `00:24:53 UTC`).

Esto respalda que el ajuste persistió a través del reinicio reportado, con el límite de que el historial no instrumenta posibles acciones manuales fuera del runner. La lectura no demuestra eficacia térmica bajo carga. El byte ICC es un objetivo de control del ventilador, no una garantía de temperatura máxima; el usuario observó más de 75 °C en un juego demandante y eso debe evaluarse por separado.

Para repetir C: aplicar fan60 universal y verificar GET `0x3C`; reiniciar manualmente; recargar GoldHEN; ejecutar inmediatamente `python tools/run_measurement.py --label C --confirm-rebooted` sin aplicar otro fan payload entre el reinicio y la sonda. El agente no reinicia la consola por software.

## F0 — disable idempotente: COMPLETADO

El código fuente actual reconoce un señuelo solo si `lstat` confirma la ruta canónica exacta y que es directorio, y `getdents` confirma que solo contiene `.` y `..`; no exige UID ni modo. Un fichero regular es un caso destructivo: disable lo elimina antes de crear el directorio y se perdería cualquier PUP descargado. Symlink, tipo no regular/no directorio, contenido dentro del directorio o fallo de lectura aborta antes de modificar.

En el intento 4, [probe_F_idempotence4.txt](../analysis/probe_F_idempotence4.txt), ambas rutas eran directorios root-owned `0555`, vacíos. El BIN de ese intento informó `DISABLED` y `existing_root_empty_directory_preserved` para ambas; la sonda antes/después mostró las mismas rutas, modo y conteo cero. F0 quedó verificada como preservación idempotente bajo la versión de código incluida en ese BIN. La medida F [probe_F.txt](../analysis/probe_F.txt) verificó después enable→rutas ausentes→disable→ambas rutas recreadas vacías. El caso mixto queda **cubierto por composición de dos ramas verificadas por separado**, no como ejecución mixta de hardware. El intento 3 anterior abortó por punteros absolutos inválidos en las rutas del BIN PIC (`EFAULT=14`); la sonda posterior confirmó que no cambió el filesystem. Se cambió la tabla de rutas a matriz contigua, se recompiló y el intento 4 pasó.

En la prueba histórica la propiedad comprobada era UID 0 más modo permitido y vacío en runtime; esa prueba no identifica al creador original. Enable usa exclusivamente `rmdir` sobre las rutas exactas tras repetir la inspección; nunca borra recursivamente ni elimina ficheros. La prueba funcional previa del usuario sigue registrada: con disable activo y NP environment `np`, la consola detectó el update y mostró “No se ha podido descargar”.

## F — roundtrip enable → sonda → disable → sonda: COMPLETADO

La ejecución histórica de F partió después de F0; el runner requería preflight de ambas rutas como directorios root-owned vacíos `0777` o `0555`, descargas automáticas desactivadas y confirmación explícita de **Debug Settings → PlayStation Network → NP environment = `noupdt`**. Ese criterio pertenecía al BIN/runner usado en esa prueba, no al fuente corregido ahora:

```powershell
python tools/run_measurement.py --label F --confirm-auto-downloads-off --confirm-np-noupdt --ack-update-risk
```

El modo F limita cada espera de conexión/reporte a 5 segundos, no introduce pausas entre operaciones y ante error después de enable intenta una sola restauración disable. Se ejecutó conforme a la instrucción del usuario con descargas automáticas desactivadas y el operador confirmó `noupdt` mediante `--confirm-np-noupdt`; el payload no puede leer el registro para verificarlo por sí mismo. En [probe_F.txt](../analysis/probe_F.txt), enable quitó ambas rutas vacías con `rmdir` exitoso y disable las recreó como directorios vacíos `0777`. La sonda final confirmó ambas rutas y el byte ICC 79. El roundtrip está verificado en esta consola; no se intentó descargar un PUP durante F.

## F_RECHECK — BIN reconstruidos con criterio ruta/tipo/vacío: COMPLETADO

Tras reconstruir únicamente `disable_updates_universal.bin` y `enable_updates_universal.bin`, el runner volvió a verificar sus SHA antes de cada envío. Los hashes fueron `c7cb5370041a982a54cb1cb2cc1c004d435451f5d0b376d93c372cee47e4541a` (enable) y `0a3ae44cf6a691c17432b734687a5d386bc5561298985e882a92b5f6d381db62` (disable). El informe completo, incluido en [probe_F_recheck.txt](../analysis/probe_F_recheck.txt), registra:

1. Sonda inicial: ambas rutas eran directorios vacíos `0777`.
2. Enable: reconoció ambas como `EXACT_PATH_EMPTY_DIRECTORY`, ejecutó `rmdir` con retorno 0 y dejó ambas ausentes.
3. Disable: recreó ambas rutas y reportó `DISABLED`.
4. Sonda final: ambas eran directorios vacíos `0777`.

La medida terminó sin errores. La detección ya no usa ni propietario ni permisos; el estado final confirma el roundtrip para estas rutas en PS4 Pro 9.00 + GoldHEN 2.4b18.10. No se intentó descargar un PUP durante la medida.

## Procedimiento C antiguo y D/E

Los informes A/B/D/E están en `analysis/probe_A.txt`, `probe_B.txt`, `probe_D.txt`, `probe_E.txt` y `probe_E2.txt`. E1 tuvo un fallo del verificador, ejecutó rollback exacto a 79 y fue corregido antes de E2. No repitas E ni uses un binario que no esté en allowlist.

# Auditoría de C y F0 — 2026-11-20

## Alcance de las ramas de F0

El escenario mixto originalmente previsto (PUP ausente y `.net.temp` existente antes de un único disable) no se ejecutó como una sola medida. Queda **cubierto por composición de dos ramas verificadas por separado**: F0 preservó rutas existentes verificadas como vacías; F ejecutó `enable` y luego `disable`, verificando la retirada de las dos rutas y la recreación de ambas. `run_disable` procesa las rutas independientemente, sin interacción entre ellas, así que la combinación no introduce una rama nueva. No se afirma que una ejecución mixta específica se haya observado en hardware.

La lectura estática de `payloads/updates_control.c::run_disable` confirma el comportamiento compuesto: primero inspecciona ambas rutas sin mutarlas; luego conserva cada directorio vacío reconocido y crea cada ruta ausente. La rama de preservación se verificó en F0 y la rama ausente→crear se verificó en F. Caso mixto cerrado por composición; no requiere BIN auxiliar, allowlist nueva ni escritura FTP.

## C: estado ICC tras reinicio

El informe crudo se conserva sin editar en [probe_C.txt](probe_C.txt). La invocación registrada indica:

```text
MEDIDA C intento 1
SEND key=probe ... sha256=718f1d0d68dd5a2a12d5e7535121bb6ca0831d96ce55b401c6244103a656e61d
```

No aparece ningún envío de fan payload en C. El usuario había informado que ya reinició la consola; al ejecutar la sonda el proceso tenía UID/GID 0 y sandbox inactivo, evidencia de que GoldHEN estaba cargado. La sonda se guardó a `2026-11-20 00:18:54 UTC`.

| Momento | Evidencia |
|---|---|
| Antes del reinicio reportado | E2 aplicó `fan_threshold_60_universal.bin`, SHA-256 `7ee35577a903899037630ad5d201dc9a03aa480dbf9b757753186375ce702ad0`, y verificó SET/GET en 60. |
| Después del reinicio reportado | C envió solo la sonda. GET1 y GET2 estables: byte 5=`0x3C`. `/update/PS4UPDATE.PUP` y `.net.temp`: ambas directorios `0777`. |
| Después de C | Los smoke tests empezaron a `00:24:53 UTC`; TDEFAULT aplicó 79 a `00:26:18 UTC` con `fan_default_universal.bin` SHA-256 `d2d26d032bccbc0aa9499ad9ab71549c7569a584264074f4f804c13d148f46b2`. |

**Conclusión acotada:** la evidencia respalda que el byte ICC=`0x3C` persistió a través del reinicio reportado. No hay envío de fan entre el reinicio declarado y C en el historial del runner; las escrituras posteriores de T50…TDEFAULT ocurrieron después de C. No se puede auditar desde el host una acción manual que no haya quedado registrada. La temperatura de un juego por encima de 75 °C no contradice la lectura del byte: el umbral ICC es un objetivo de ventilador, no una cota térmica ni una garantía de eficacia bajo cualquier carga.

## F0 intento inicial (histórico): abortado sin modificar rutas

El usuario confirmó descargas automáticas desactivadas y autorizó F0/F. En el preflight en vivo de F0, la sonda registró:

```text
update.pup.stat.rc=-1
update.pup.errno=2
update.net_temp.stat.rc=0
update.net_temp.permissions_octal=0777
update.net_temp.type=directory
```

El runner detuvo la secuencia tras el primer informe. `analysis/probe_F_idempotence.txt` muestra solo `SEND key=probe`; **no envió `disable_updates_universal.bin`**. Así que esta ejecución no retiró ni creó rutas. No alcanzó el criterio de idempotencia de dos señuelos existentes.

## Actualización: B, enable original y F0 ampliado

El usuario confirmó inicialmente y volvió a confirmar el 2026-11-19 que al aplicar el fan60 original apareció la notificación y el ventilador aumentó audiblemente. El registro B describe el cambio ICC del byte `0x05`, `0x4F`→`0x3C` (1 de 28 bytes); no se adjuntó el log TCP crudo de esa medida. La observación audible/notificación confirma el efecto reportado por el usuario y concuerda con el cambio de GET registrado; no se presenta como log crudo capturado por el runner.

El usuario informa que ejecutó el `enable_updates` original de 9.00 y luego intentó descargar una actualización. La hipótesis más probable para que `/update/PS4UPDATE.PUP` apareciera ausente es que enable retiró el señuelo y el intento de descarga del sistema limpió esa ruta antes de escribir un PUP. Es una explicación probable apoyada por la secuencia reportada, no una causalidad demostrada por telemetría.

El desensamblado original `analysis/disasm/9.00_enableupdate.bin.asm` confirma que `_main` llama al wrapper syscall 10 (`unlink`) y después al wrapper syscall 137 (`rmdir`) para `/update/PS4UPDATE.PUP.net.temp` (llamadas desde offsets `0x2f` y `0x40`); repite `unlink` y `rmdir` para `/update/PS4UPDATE.PUP` (offsets `0x4c` y `0x58`). En esos sitios los retornos no se prueban antes de pasar a la operación siguiente. Por tanto, intenta retirar ambas rutas, pero no valida que ambas hayan desaparecido: directorios no vacíos u otros errores pueden dejar una ruta presente sin que el flujo principal lo detecte.

La sonda ampliada `probe/build/goldhen_readonly_probe.bin` tiene SHA-256 `5ef18fa92d57ddf703340ad970283605f76ee6bc84b60799f28caa35488f4b20` (113.424 bytes). El reporte `probe_F_idempotence2.txt` muestra ambas rutas como directorios UID 0, modo `0555`, `entries.count=0`, `entries.status=OK`. El FTP de solo lectura confirmó el modo y listó únicamente `.` y `..` para ambas rutas, en EPSV y PASV; `/dev/icc_fan` es un dispositivo de caracteres `0666`.

En ese intento anterior, el preflight todavía exigía `0777`; solo se envió la sonda y se detuvo. Esa regla fue revisada después de verificar UID 0 y cero entradas por FTP y getdents. Véase F0 intento 4 más abajo para el criterio y resultado actualizado.

La lectura ICC del mismo reporte fue `0x4F` dos veces estable: al momento de F0 el byte 5 estaba en 79, tras el smoke test de default. Esto no contradice el B anterior.

La captura histórica F0 está terminada; el criterio actualizado aplica al fuente presente. Los BIN de release conservan sus hashes y no incorporan este cambio hasta que se autorice una nueva compilación y publicación.

## F0 intento 4 — completado sin cambiar las rutas

El intento 3 envió el BIN disable SHA `32614e4cec4f24554104a2ac84d0231267a0c4587aa6bce345006dfe8b6fc9d2`, pero el payload abortó antes de operar: la tabla C `const char *const update_paths[]` produjo punteros absolutos inválidos en el BIN raw PIC; `lstat` recibió ruta vacía y devolvió `EFAULT` (14). La sonda posterior confirmó que ambas rutas seguían como directorios root-owned `0555`, vacíos; no hubo cambios de filesystem.

Se corrigió la tabla a una matriz contigua `char[2][36]`, recompiló el BIN y se verificó ELF `ET_DYN` sin secciones de relocación, referencias RIP-relative a las rutas, hashes y syscalls permitidas. El intento 4 guardado en [probe_F_idempotence4.txt](probe_F_idempotence4.txt) envió la sonda, disable y la sonda final; el payload informó `updates.status=DISABLED`. Para ambas rutas, `inspect_route` confirmó UID 0, modo `0555`, entradas vacías; la acción fue `existing_root_empty_directory_preserved`. Antes y después siguen directorios `0555`, vacíos (`entries.count=0`). F0 quedó completo como operación idempotente sin cambios de rutas; no ejercitó la rama de ruta ausente ni la creación `0777`, porque PUP ya existía como directorio.

El criterio corregido de reconocimiento es ruta canónica exacta + tipo directorio + contenido vacío, sin exigir UID propietario ni permisos concretos. No se afirma probar quién creó originalmente la ruta; `enable` solo hace `rmdir` después de repetir la comprobación de vacío, nunca borrado recursivo. La revalidación de BIN reconstruidos aparece en F_RECHECK abajo.

## F: roundtrip completado

El runner ejecutó `tools/run_measurement.py --label F --confirm-auto-downloads-off --confirm-np-noupdt --ack-update-risk` conforme a la instrucción del usuario de usar `noupdt` durante F. La preferencia no se lee desde el payload; el flag registra la confirmación del operador, no es lectura independiente del registro.

El informe [probe_F.txt](probe_F.txt) registra la secuencia y hashes verificados justo antes de cada envío:

| Paso | Resultado |
|---|---|
| Sonda inicial | Ambas rutas root-owned `0555`, vacías; ICC byte 5=`79` |
| Enable (`2e1f0ad4…3c1432a1`) | `rmdir` rc 0 en las dos rutas; `ENABLED_FILES_NOT_TOUCHED` |
| Sonda tras enable | Ambas rutas ausentes (`errno=2`) |
| Disable (`32614e4c…8b6fc9d2`) | Crea las dos rutas canónicas; `DISABLED` |
| Sonda final | Ambas rutas directorios root-owned `0777`, vacíos (`entries.count=0`); ICC byte 5=`79` |

La validación del runner terminó `Medida F completa`, sin errores ni recuperación de emergencia. El roundtrip quedó verificado en PS4 Pro 9.00 + GoldHEN 2.4b18.10. No se ejecutó ninguna prueba de descarga durante el ciclo; la evidencia funcional previa reportada por el usuario está en `analysis/UPDATES_TESTS.md`.

## Comprobación FTP posterior, solo lectura

Tras F se usó el servidor FTP de GoldHEN (banner `GoldHEN FTP server v2.2`) en modo pasivo únicamente con `LIST` sobre `/dev`, `/update` y las dos rutas PUP. `/dev/icc_fan` apareció como dispositivo de caracteres root-owned con permisos `0666`. `/update/PS4UPDATE.PUP` y `.net.temp` siguen siendo directorios root-owned `0777`; el `LIST` individual de cada uno mostró solo `.` y `..`. No se ejecutó ningún comando FTP de escritura. Esta lectura confirma el estado después de F. El caso mixto queda cubierto por composición de ramas, aunque no se tomó una captura mixta específica.

## F_RECHECK: bins reconstruidos

Después de habilitar NP environment `noupdt` y confirmar descargas automáticas apagadas, se reejecutó la secuencia con los dos nuevos BIN y sus hashes comprobados inmediatamente antes de cada envío. La captura completa es [probe_F_recheck.txt](probe_F_recheck.txt):

- Preflight: ambas rutas exactas eran directorios vacíos `0777`.
- Enable reconstruido (`c7cb5370041a982a54cb1cb2cc1c004d435451f5d0b376d93c372cee47e4541a`): ambos `rmdir` devolvieron 0; la sonda confirmó que ambas rutas quedaron ausentes. El log clasificó cada objetivo como `EXACT_PATH_EMPTY_DIRECTORY`, sin condición de UID o modo.
- Disable reconstruido (`0a3ae44cf6a691c17432b734687a5d386bc5561298985e882a92b5f6d381db62`): informó `DISABLED`; la sonda final confirmó dos directorios vacíos.

La revalidación terminó correctamente en PS4 Pro 9.00 + GoldHEN 2.4b18.10. No se ejecutó una descarga de PUP durante el roundtrip.

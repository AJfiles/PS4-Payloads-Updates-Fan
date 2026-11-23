# Estado autoritativo de pruebas

Consola: PS4 Pro, firmware 9.00, GoldHEN 2.4b18.10. El runner registra hashes justo antes de cada envío. Un envío y su log solo acreditan el comportamiento observado en esta consola; no demuestran otras combinaciones de modelo o firmware.

| Prueba | Evidencia | Resultado |
|---|---|---|
| Fan 60 original | [probe_B.txt](probe_B.txt) | El usuario confirmó notificación y aumento audible del ventilador; el reporte ICC muestra byte 5 de 79 a 60. |
| Persistencia fan tras reinicio | [probe_C.txt](probe_C.txt) | Tras reinicio manual y recarga de GoldHEN, GET leyó 60 dos veces. |
| Fan default original | [probe_D.txt](probe_D.txt) | GET leyó 79; default aplica 79 y no restaura snapshot. |
| Fan universal | [probe_E2.txt](probe_E2.txt), [FAN_SMOKE_TESTS.md](FAN_SMOKE_TESTS.md) | 60 y 50/55/65/70/75/80/default terminaron con GET verificado; se cambió solo byte 5. 80 supera el stock observado 79. |
| F0 histórico | [probe_F_idempotence4.txt](probe_F_idempotence4.txt) | El BIN de entonces preservó señuelos vacíos `0555`; su precondición aún dependía de UID/modo. Es evidencia histórica, no del BIN reconstruido. |
| F histórico | [probe_F.txt](probe_F.txt) | Enable→disable completado con los BIN anteriores; conservado como historial separado. |
| F_RECHECK actual | [probe_F_recheck.txt](probe_F_recheck.txt) | Los BIN reconstruidos pasaron la secuencia sonda previa→enable→sonda→disable→sonda. Enable quitó dos directorios vacíos `0777`; disable los recreó; la sonda final confirmó ambos vacíos. |
| Auditoría de BIN y hashes | Salida de `tools/audit_universal_payloads.py`; [SHA256SUMS.txt](../payloads/build/SHA256SUMS.txt) | 10/10 BIN concuerdan con manifiesto; 8 hashes fan intactos; los 2 hashes de updates reflejan la reconstrucción. Auditoría estática de syscalls pasó. |

## BIN de updates reconstruidos

| Archivo | Bytes | SHA-256 |
|---|---:|---|
| `disable_updates_universal.bin` | 11080 | `0a3ae44cf6a691c17432b734687a5d386bc5561298985e882a92b5f6d381db62` |
| `enable_updates_universal.bin` | 10632 | `c7cb5370041a982a54cb1cb2cc1c004d435451f5d0b376d93c372cee47e4541a` |

La nueva precondición de enable es ruta canónica exacta + directorio + vacío, sin filtro por modo o propietario. El estado observado durante F_RECHECK fue `0777`; no se probó una ruta `0555` con los BIN reconstruidos, pero el código ya no inspecciona esos bits ni UID. `rmdir` sigue limitado a esas rutas, y falla si contienen entradas.

## Discrepancia de modo

**NO RESUELTA.** Los originales pasan `0x309` (decimal 777, octal `01411`) a `mkdir`. `umask` puede quitar bits, no convertir ese valor en los modos observados `0777` o `0555`. Las explicaciones posibles son `chmod` posterior, normalización del filesystem o creación de las rutas por otro payload. El modo es irrelevante para bloquear el PUP: importa que la ruta sea un directorio y no un fichero. La hipótesis de que `0555` impida `rmdir` está descartada por [FreeBSD `rmdir(2)`](https://man.freebsd.org/cgi/man.cgi?query=rmdir&sektion=2&manpath=FreeBSD+14.0-RELEASE), que exige directorio vacío y comprueba permisos del padre.

Durante el roundtrip el operador confirmó `NP environment = noupdt` y descargas automáticas desactivadas. El primero no fue leído independientemente por el payload. No se intentó descargar un PUP durante F_RECHECK.

## Pruebas de Fase 2 (v2 multicapa y registro de sistema)

| Prueba | Evidencia | Resultado |
|---|---|---|
| Diferencial de registro: Baseline | [probe_np_dual_baseline.txt](probe_np_dual_baseline.txt) | Hardware: Menú en `noupdt` y descargas automáticas OFF. Medido: `0x19800000` = `noupdt`, `0x0780B007` = `np`, `0x02800500` = `0` (`rc=0, errno=0`). |
| Diferencial de registro: Estado ON | [probe_np_dual_on.txt](probe_np_dual_on.txt) | Hardware: Menú en `np` y descargas automáticas ON. Medido: `0x19800000` = `np`, `0x0780B007` = `np`, `0x02800500` = `1` (`rc=0, errno=0`). Claves y valores confirmados empíricamente al 100%. |
| Paso 1: `write_verify_noop.bin` | [probe_write_verify_noop.txt](probe_write_verify_noop.txt) | Test de escritura y relectura sin cambio de estado: `auto_download=1` y `NP_env="np"` (17 bytes con relleno cero). Ambas escrituras con `rc=0, errno=0`, relectura idéntica, `bytes_match=yes`. No tocó filesystem. `write_verify.status=PASS_ALL`. |
| Paso 3: `disable_updates_full.bin` | [probe_disable_updates_full.txt](probe_disable_updates_full.txt) | Bloqueo en 3 capas: Capa 1 preservó señuelos vacíos; Capa 2 cambió `auto_download` de 1 a 0; Capa 3 cambió `NP_env` de `np` a `noupdt`. Notificación en pantalla `Updates: 3/3 aplicado`. `updates.status=ALL_LAYERS_APPLIED (3/3)`. |
| Paso 4: `enable_updates_full.bin` | [probe_enable_updates_full.txt](probe_enable_updates_full.txt) | Reversión en 3 capas: Capa 1 eliminó señuelos con `rmdir` (quedaron `missing`); Capa 2 cambió `auto_download` de 0 a 1; Capa 3 cambió `NP_env` de `noupdt` a `np`. Notificación `Updates: 3/3 restaurado`. `updates.status=ALL_LAYERS_RESTORED (3/3)`. |
| Estado final protegido: Reaplicación disable | [probe_disable_updates_full.txt](probe_disable_updates_full.txt) | Reaplicó `disable_updates_full.bin`: recreó señuelos desde missing (`mkdir_missing`, `rc=0`), `auto_download=0`, `NP_env="noupdt"`. Consola queda en estado final 100% protegido. |
| Inmediatez del corte de red | Prueba funcional en vivo del usuario | **VERIFICADO**: Comprobación en *Ajustes > Actualización del software del sistema* falla de inmediato sin detectar actualización (Caso A). El shell no cachea el entorno NP y el corte opera en caliente sin requerir reinicio. |
| Persistencia tras reinicio de escrituras payload | [probe_np_dual_post_reboot.txt](probe_np_dual_post_reboot.txt) | **VERIFICADO**: Sonda dual tras reinicio completo y recarga de GoldHEN midió `NP_env="noupdt"` y `auto_download=0` (`rc=0, errno=0`). Las escrituras de registro persisten tras reinicio completo y recarga de GoldHEN (verificado) en `/system_data/settings/system.dat` sin requerir reaplicar el payload tras reiniciar. |

### Manifiesto de binarios v2 (`payloads/build_v2/SHA256SUMS.txt`)

```text
9fcde094b5c0fa6134a9bee0f1823a4506f9549b55778e892f232a101289c6bb  write_verify_noop.bin
d164f3ee4df8e42b67a979f4e5c00101789a842615610273c7046bf9a1ad1043  disable_updates_full.bin
714ed6707080f5a70c00d1c2ca8d9b7e8ad2988ac39dfbac6a9123cacbcd6990  enable_updates_full.bin
a8722089a2e79183480af16bbed9989df88e337a63b0311be34d051f49c90b80  np_readonly_dual_probe.bin
```


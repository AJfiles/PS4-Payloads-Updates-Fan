# Notas de Diseño y Arquitectura — Proyecto GoldHEN / PS4

## 1. Estado y artefactos

Proyecto terminado y verificado en hardware para **PS4 Pro, firmware 9.00, GoldHEN 2.4b18.10**. Raíz del proyecto: `AntiUpdatesPS4`.

- Diez BIN: `payloads/build/` (siete umbrales 50–80, default y dos de updates).
- Sonda de solo lectura: `probe/build/goldhen_readonly_probe.bin`.
- Paquete congelado: `dist/goldhen_universal_payloads.zip`.
- Manifiesto autoritativo: `payloads/build/SHA256SUMS.txt`.
- Evidencia de prueba: `analysis/TEST_STATUS.md` y los informes `analysis/probe_*.txt`.

Hashes copiados literalmente del manifiesto:

```text
c09533b4811fd49642a1d6bdea84973da0227aa80c5c3f38f8fdbcaa76fbf549  fan_threshold_50_universal.bin
e8763eaae27248138ea1d37234919acaba93db1a13b0b1ee1f089695ae69a54f  fan_threshold_55_universal.bin
7ee35577a903899037630ad5d201dc9a03aa480dbf9b757753186375ce702ad0  fan_threshold_60_universal.bin
6e3d3f0f87e687a2f8f3ab202da3446fdd3a32e40a4784027e75dbd91ec2197a  fan_threshold_65_universal.bin
31f8ab3898134dda855e2737c656b387e7738bf8a997f6bc5939c5590827b017  fan_threshold_70_universal.bin
76676ff8d86382c95cab5d6ef4d07df25e5fdb79112566246c845c42c6f07df8  fan_threshold_75_universal.bin
bf31765ed3de71955c5a77aa101cf1118dabd0bb587f3d43c11c35cb397a3e94  fan_threshold_80_universal.bin
d2d26d032bccbc0aa9499ad9ab71549c7569a584264074f4f804c13d148f46b2  fan_default_universal.bin
0a3ae44cf6a691c17432b734687a5d386bc5561298985e882a92b5f6d381db62  disable_updates_universal.bin
c7cb5370041a982a54cb1cb2cc1c004d435451f5d0b376d93c372cee47e4541a  enable_updates_universal.bin
```

Cross-check: disable, enable y ZIP coinciden con los valores de control del handover. La lectura SHA-256 del estado actual también confirmó que los diez BIN coinciden con el manifiesto y que el ZIP coincide con `ad8935ad3736bcca77f88a0d9b6c7256e329a21a330f07cfd1fc9a15ab7e9a73`. Los diez BIN coinciden con el manifiesto y el ZIP coincide con su hash verificado.

## 2. Entorno de pruebas

- PS4 Pro, FW 9.00, GoldHEN 2.4b18.10. Es la única consola de prueba.
- PS4: `<PS4_IP>`; BinLoader TCP `9090`, FTP TCP `2121`.
- PC: `<PS4_IP>` (actualizado desde la anterior IP `.48`); receptor TCP `9090`. Verificar el PC con `ipconfig` antes de configurar otro envío. La sonda histórica congelada (hash `5ef18fa9...`) tiene incrustada la IP `.48` y ya no reportará por TCP; **no reconstruirla ni modificarla**.
- `sceKernelGetSystemSwVersion = " 9.008.031"`, numérico `0x09008031`; `kern.sdk_version = 31 80 00 09`, el mismo valor little-endian.
- El banner FTP `GoldHEN FTP server v2.2` es la versión del servidor FTP, **no** la versión de GoldHEN.

## 3. Los dos mecanismos — ground truth, no re-derivar

**Fan:** `/dev/icc_fan` existe como dispositivo de caracteres con permisos `0666`. SET `0xC01C8F07` y GET `0xC01C8F08` son `_IOWR`, grupo `0x8F`, longitud codificada 28 bytes. Byte 5 es el umbral; el stock observado es `0x4F` = 79. Tabla stock completa medida en A:

```text
00 00 00 00 00 4f 00 00 00 08 00 00 80 00 00 00 ff ff ff 0f ff ff ff 0f ff ff ff 0f
```

En B, el fan 60 original cambió solo byte 5; la sonda dejó estables los otros 27 bytes. El kernel ignoró los demás campos de entrada para el efecto observado en esta consola. C mostró que el ajuste persiste tras reiniciar y recargar GoldHEN. Es un objetivo del ventilador, no un techo duro de temperatura.

**Updates:** señuelos son directorios vacíos en `/update/PS4UPDATE.PUP` y `/update/PS4UPDATE.PUP.net.temp`. Disable elimina con `unlink` el fichero regular real si existe, luego crea el directorio; por tanto, un PUP que esté ahí se pierde. Enable solo hace `rmdir` de los directorios vacíos en esas rutas exactas. La modificación del filesystem persiste al reiniciar. Bloquea el PUP de **sistema**, no parches de juegos ni apps. El usuario vio que la consola detecta la actualización y al intentar descargarla muestra “No se ha podido descargar”.

## 4. Arquitectura y por qué

Un source por función genera N BIN. Los payloads nuevos no llevan offsets de kernel, `kexec` ni syscall 11. Comprueban en runtime UID 0 y sandbox inactivo; si falla la guarda, abortan sin tocar estado. El fan hace GET → conserva los 28 bytes → modifica solo byte 5 → SET → GET de verificación; usa buffer cero de 64 bytes y rollback exacto si la verificación detecta otro cambio. Si GET falla, no escribe. Updates identifica el señuelo por ruta exacta + tipo directorio + vacío; no consulta modo ni propietario. La portabilidad depende de las interfaces disponibles —nodo ICC, contexto privilegiado y rutas—, no de resolver offsets por firmware.

## 5. Hipótesis descartadas

- `0x0780B007` y `0x0781B007`: **`SCE_REGMGR_ENT_KEY_USER_01_16_NP_env(idx)`**, NP Environment **DE ÁMBITO USUARIO** (`idx << 16`), NO el ajuste global de Debug Settings. Descartadas empíricamente tras tres lecturas de hardware sin cambio (`np` persistente) y confirmadas mediante inspección de símbolos de `Sce.Vsh.Registry.dll.sprx`. El riesgo de escribir en este rango es romper el inicio de sesión PSN del usuario, no destruir el perfil.
- **Hallazgo clave de ámbito en NP Environment:** AMBAS claves se denominan `NP_env`; lo que las distingue es su ámbito:
  - Ámbito Sistema (Debug Settings / global): **`SCE_REGMGR_ENT_KEY_NP_env = 0x19800000`** (427819008, tipo String, 17 bytes).
  - Ámbito Usuario (por cuenta local): **`SCE_REGMGR_ENT_KEY_USER_01_16_NP_env(idx)`** (`0x0780B007` para slot 1).
- escribir en el rango per-usuario `0x0780B000..0x0790B000`.
- Escaneo por firmas de kernel: **innecesario**, no hay offsets que resolver en la arquitectura actual.
- Adquirir kernels, dumps o PUP: **cancelado; no aplica**.
- `0555` más resistente que `0777`: **falso**. `rmdir` depende de que el directorio esté vacío y de los permisos del padre, no de los permisos del directorio eliminado ([manual FreeBSD `rmdir(2)`](https://man.freebsd.org/cgi/man.cgi?query=rmdir&sektion=2&manpath=FreeBSD+14.0-RELEASE)). Cierre formal completado por matriz unitaria 71/71 (`analysis/DECOY_DECISION_TEST.md`).
- “El original degrada la tabla ICC”: **falso** en la PS4 de prueba; los otros campos no cambiaron al aplicar el SET original.
- Reproducción byte-idéntica de los originales: no era requisito y se descartó; importa equivalencia funcional y seguridad.
- Inferir GoldHEN desde el banner FTP: **falso**.

## 6. Bugs de los payloads originales — no restaurar

- Los originales fan pasan un buffer de 10 bytes a ioctl `_IOWR` de 28 bytes: el kernel lee 18 bytes fuera del buffer y copia 18 bytes de vuelta más allá del objeto de pila.
- Los originales de updates pasan `777` decimal (`0x309`, octal `01411`) a `mkdir`, no `0777` octal.
- Los fan originales no hacen GET previo: escriben a ciegas.
- El enable original no comprueba los retornos ni verifica que las dos rutas desaparezcan.
- Los BIN originales contienen stub `kexec`/syscall 11 con datos por firmware; de ahí la segmentación por versiones.
- **NO RESUELTO:** `0x309/01411` no explica los modos observados `0777` y `0555`; `umask` solo puede quitar bits. Posibles causas: `chmod` posterior, normalización del filesystem u otro payload creó los directorios. Mantener abierto.
- El repo WPA-Libertario comparte los defectos de buffer corto y ausencia de GET; ofrece umbrales 60/70/80/85. 85 es aún mayor que el stock observado 79 y peor como objetivo para esta consola.

## 7. Tabla de pruebas

Detalle autoritativo en [analysis/TEST_STATUS.md](analysis/TEST_STATUS.md). Las pruebas acreditan PS4 Pro 9.00 + GoldHEN 2.4b18.10, no otros modelos/firmwares.

| Medida | Qué probó | Evidencia |
|---|---|---|
| A — sonda inicial | Identidad, firmware, ICC GET estable, tabla stock 79 y rutas | `analysis/probe_A.txt` |
| B — fan 60 original | Byte 5 `79→60`, otros bytes iguales; notificación y aumento audible confirmados por usuario | `analysis/probe_B.txt` |
| C — persistencia | Tras reinicio manual y recarga de GoldHEN, byte 5=60 en dos GET | `analysis/probe_C.txt` |
| D — default original | Aplicó 79; no restaura una instantánea previa | `analysis/probe_D.txt` |
| E1 — fan universal inicial | El comparador falló tras SET correcto; rollback exacto se comprobó en hardware y volvió a 79 | `analysis/probe_E.txt` |
| E2 — fan60 universal corregido | GET/SET verificados, solo byte 5 modificado | `analysis/probe_E2.txt` |
| F0 histórico | Disable idempotente sobre directorios vacíos `0555`; esa build todavía filtraba por UID/modo | `analysis/probe_F_idempotence4.txt` |
| F histórico | Roundtrip con BIN previos; conservar como historial | `analysis/probe_F.txt` |
| F_RECHECK | BIN reconstruidos: enable → sonda ausente → disable → sonda confirma dos directorios vacíos | `analysis/probe_F_recheck.txt` |
| Smokes de los ocho presets | 50, 55, 60, 65, 70, 75, 80, default 79; cada GET confirmó solo byte 5 | `analysis/probe_T50.txt`, `probe_T55.txt`, `probe_E2.txt`, `probe_T65.txt`, `probe_T70.txt`, `probe_T75.txt`, `probe_T80.txt`, `probe_TDEFAULT.txt`; resumen `analysis/FAN_SMOKE_TESTS.md` |

E1 no fue éxito funcional del umbral final: el bug del comparador sí probó que el rollback restauró exactamente el estado anterior. Distinguir esa validación de rollback de la prueba exitosa E2.

## 8. Reglas de seguridad — incidentes reales

- **Incidente de destino:** un build anterior de la sonda apuntaba a `<PS4_IP>:9090`, la propia PS4. El texto del informe habría ido al BinLoader y este puede intentar ejecutarlo como payload. El destino del informe debe ser siempre la IP del PC, verificada con `ipconfig`; nunca incrustar la IP de PS4 como `--report-host`.
- No usar `Test-NetConnection` ni conectar y cerrar contra PS4:9090; puede consumir el `accept` del loader.
- FTP solo con `LIST`, `STAT`, `SIZE`, `RETR`, limitado a `/dev`, `/update`, `/mnt/usb0`. Prohibidos `STOR`, `DELE`, `RMD`, `MKD`, `RNFR`, `RNTO`, `SITE` y cualquier escritura.
- No reiniciar la consola por software. La sonda no escribe en el HDD interno.
- Verificar allowlist SHA-256 antes de cada envío. Los BIN nuevos requieren OK explícito del usuario antes de cargarlos.
- Para una prueba de enable, descargas automáticas de sistema OFF y preferiblemente NP environment=`noupdt`. Mantener la consola despierta y no aceptar avisos.
- No ejecutar medidas ya cerradas ni pedir que se vuelvan a entregar sus reportes.

## 9. Tooling y ejecución de medidas

- Venv del proyecto: Python 3.12.10 + Capstone 5.0.9.
- Toolchains y herramientas registradas en `research/TOOLING.md`: Zig 0.13.0 portable, OpenOrbis PS4 Toolchain v0.5.4 y herramientas locales de análisis.
- Scripts: `tools/run_measurement.py`, `tools/send_payload.py`, `tools/receive_probe.py`, `tools/audit_*.py`, `tools/build_universal_payloads.py` y `tools/package_release.py`.
- Para cualquier medida futura, desde la raíz: iniciar primero el receptor con `python tools/receive_probe.py --host 0.0.0.0 --port 9090`; luego usar el runner que verifica SHA y registra captura. Las medidas de este proyecto ya están cerradas; no volver a ejecutarlas sin nueva autorización explícita. Los paths con espacios deben ir entre comillas en PowerShell.
- Regla de Firewall Windows documentada para Python 3.12, TCP 9090 y subred local:

```powershell
New-NetFirewallRule -DisplayName "PS4 Readonly Probe TCP 9090" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 9090 -Program "python.exe" -Profile Public -RemoteAddress LocalSubnet
```

- Fallback manual: receptor y sender en terminales separadas. El receptor debe escuchar antes de cargar la sonda. Revisar `probe.completed=yes` y `Fin del reporte`; el sender diciendo “enviado” no demuestra que el payload haya terminado.

## 10. Huecos abiertos y Fase 2

- El hueco del modo `0555` queda formalmente **CERRADO** con la prueba unitaria local en PC (`analysis/DECOY_DECISION_TEST.md`, 71/71 casos superados). El hardware `0777` ya estaba verificado; la independencia operativa de permisos queda verificada por inspección estática del código y test unitario.
- Caso mixto (una ruta ausente y la otra presente en una misma ejecución): cubierto por composición de las ramas ya verificadas; se acepta, no requiere BIN auxiliar.
- Discrepancia `mkdir(01411)` frente a modos `0777/0555`: abierta, cosmética para la función de bloqueo.
- Fase 2a: build dual ELF para `elfldr`, puerto 9021. Upstream fijado llega a 14.00, **no 14.02**, y requiere `remote_lua_loader`; no es drop-in para la cadena actual. No prometer 14.02. Ver `analysis/DUAL_BUILD_PROPOSAL.md`.
- Fase 2b: capa NP Environment de sistema **VERIFICADA POR DIFERENCIAL EN HARDWARE** como `0x19800000` (`SCE_REGMGR_ENT_KEY_NP_env`, 427819008, tipo String, 17 bytes). En la primera lectura (menú en `noupdt`) devolvió `val_str=noupdt` (`rc=0`, `errno=0`). En la segunda lectura diferencial (menú cambiado a `np`), devolvió exactamente `val_str=np` (`rc=0`, `errno=0`), completando el diferencial de hardware. Simultáneamente, `0x0780B007` (`USER_01_16_NP_env(1)`) se mantuvo en `np` en ambas lecturas, confirmando su naturaleza exclusiva de ámbito de usuario local.
- Fase 2c: ajuste de descargas automáticas **VERIFICADO POR DIFERENCIAL EN HARDWARE** como `0x02800500` (`SCE_REGMGR_ENT_KEY_SYSTEM_UPDATE_auto_download`, 41944320, Int32). En estado OFF devolvió `val_int=0` (`0x00000000`, `rc=0`, `errno=0`); en estado ON devolvió `val_int=1` (`0x00000001`, `rc=0`, `errno=0`). Queda medido empíricamente: OFF=0, ON=1.
- Patrón de resolución de símbolos en runtime: `loadModule("libSceRegMgr.sprx")` + `dlsym` (`sys_dynlib_dlsym`) demostró funcionamiento 100% fiable en userland sin offsets de kernel ni syscalls hardcodeados. Este patrón es reutilizable para futuros desarrollos en runtime.
- Aclaración de buffers de registro: el campo oficial del registro es de 17 bytes (`SCE_REGMGR_ENT_SIZ_NP_env = 17`: 16 caracteres + null). El buffer local de stack asignado en la sonda es de 64 bytes ceros; la sonda inicial volcó 18 bytes (`hex18`) como comprobación de frontera para capturar el null byte y el byte posterior. La sonda dual ampliada normaliza la salida a `hex17` (17 bytes exactos).
- **Arquitectura Simplificada v2 (solo dos BINs nuevos, v1 congelado intacto):**
  - Se descarta generar pares sueltos por capa para no dispersar binarios. Las tres capas se integran internamente en solo dos ejecutables:
    - `disable_updates_full.bin`: Capa 1 (directorios señuelo) + Capa 2 (`auto_download = 0`) + Capa 3 (`NP_env = "noupdt"`), con reporte por capa y degradación elegante.
    - `enable_updates_full.bin`: revierte las tres (Capa 1: `rmdir` señuelos vacíos; Capa 2: `auto_download = 1`; Capa 3: `NP_env = "np"`).
  - Si `libSceRegMgr` no carga o una capa falla, jamás se aborta la Capa 1 (filesystem); se aplica lo que se pueda y se reporta individualmente.
  - Los dos BIN v1 congelados (`disable_updates_universal.bin` y `enable_updates_universal.bin`) y los 8 de fan se conservan intactos como alternativa minimalista.
  - **ESTADO DE VERIFICACIÓN EN HARDWARE (COMPLETADO):**
    - `write_verify_noop.bin` (Paso 1): validó SetInt y SetStr con len=17 sin alterar estado. Resultado: `PASS_ALL` (`analysis/probe_write_verify_noop.txt`).
    - `disable_updates_full.bin` (Paso 3): verificado en hardware, aplicó las 3 capas (`Updates: 3/3 aplicado`, `analysis/probe_disable_updates_full.txt`).
    - `enable_updates_full.bin` (Paso 4): verificado en hardware, revirtió las 3 capas (`Updates: 3/3 restaurado`, `analysis/probe_enable_updates_full.txt`).
    - **Estado actual de la consola de prueba:** `disable_updates_full.bin` reaplicado y **ACTIVO** (consola 100% protegida en las 3 capas).
    - **Inmediatez del corte de red (VERIFICADO):** Comprobado en vivo en la consola de prueba: tras aplicar `disable_updates_full.bin`, la comprobación en *Ajustes > Actualización del software del sistema* falla de inmediato sin detectar actualización (Caso A), confirmando que el shell no cachea el entorno NP y el corte opera en caliente sin reiniciar.
    - **Persistencia tras reinicio de valores escritos por payload (VERIFICADO):** Tras un reinicio completo de la consola y recarga de GoldHEN, la sonda dual `probe/build/np_readonly_dual_probe.bin` midió empíricamente: `NP_env = "noupdt"` y `auto_download = 0` (`analysis/probe_np_dual_post_reboot.txt`). Se confirma que las escrituras mediante `SetStr`/`SetInt` persisten tras reinicio completo y recarga de GoldHEN (verificado) en `/system_data/settings/system.dat` sin requerir reaplicar el payload tras reiniciar.
- Verificación comunitaria: la sonda de solo lectura y su plantilla están en `docs/PROBE.md` y `docs/PROBE_REPORT_TEMPLATE.md`.

## 11. Preferencias del usuario y quickstart

- Idioma español. Etiquetas honestas: VERIFICADO / DERIVADO / EXPERIMENTAL / NO RESUELTO. No inventar porcentajes de compatibilidad ni prometer 14.02.
- No rehacer análisis, no repetir medidas cerradas, congelar artefactos y reportar el hash final sin reconstrucciones sucesivas.
- Para carga habitual: fan60–65 como objetivos razonables según preferencia del usuario; el umbral no fija temperatura máxima. 80 está por encima del stock 79 y no es “más refrigeración”; 85 del repo de referencia es aún más alto. Para volver al valor stock observado, aplicar `fan_default_universal.bin`, que escribe 79 y no restaura snapshot.
- Updates: `disable_updates_universal.bin` ocupa las dos rutas de PUP de sistema y persiste en filesystem; `enable_updates_universal.bin` retira solo directorios vacíos en esas rutas. No cubre parches de juegos ni aplicaciones. Un fichero PUP encontrado por disable se elimina.
- Los payloads nuevos no enfrían más que los originales: conservan el objetivo funcional, añaden buffer ICC seguro, verificación y reversión, y eliminan la dependencia del fallback de kernel en la arquitectura actual.
- BinLoader GoldHEN: PS4 TCP 9090. Sonda: receptor PC TCP 9090. Antes de usar otra red, compilar la sonda hacia la IPv4 del PC y auditarla; no apuntar el reporte al puerto/IP del BinLoader.

## 12. Addendum (añadido en la migración, no generado por el agente anterior)

### 12.1 Objetivo original del proyecto
Dos payloads universales de actualizaciones (activar/desactivar) y siete de ventilador
(50/55/60/65/70/75/80) más default, desde un único código fuente por función, para no mantener
un binario por firmware. Cumplido y verificado en la consola de pruebas.

### 12.2 Hash de la sonda (falta en la sección 1)
El manifiesto payloads/build/SHA256SUMS.txt tiene ONCE entradas: 10 payloads + la sonda.
Último valor conocido de la sonda ampliada (113.424 bytes):
5ef18fa92d57ddf703340ad970283605f76ee6bc84b60799f28caa35488f4b20  goldhen_readonly_probe.bin
Verificar contra el manifiesto; si difiere, el manifiesto manda.

### 12.3 INCIDENTE PROPIO: EFAULT=14 y la regla de PIC (crítico para fase 2)
Durante F0, el intento 3 abortó con EFAULT=14. Causa: el BIN crudo usaba una TABLA DE PUNTEROS
ABSOLUTOS a las cadenas de rutas. Un payload crudo es position-independent y no tiene
relocalaciones: esos punteros no se reubican al cargarse, apuntan a basura.
Arreglo aplicado: sustituir la tabla de punteros por una MATRIZ CONTIGUA de caracteres
(array de arrays), indexada por offset relativo.
en BIN crudo, nunca tablas de punteros absolutos a datos. Usa matrices contiguas o
offsets relativos. Esta es la trampa más probable al escribir los payloads nuevos de fase 2.
La auditoría posterior confirmó: ELF intermedios ET_DYN, sin relocalizaciones pendientes ni
operandos de memoria absolutos. Ver analysis/PIC_AUDIT.md.
Nota: un ELF para elfldr no tiene este problema, porque sí tiene relocalaciones.

### 12.4 BinLoader: hay que activarlo en la consola
Ruta del menú: Settings > ★ GoldHEN > Server Settings > Enable BinLoader Server.
Si está desactivado, el envío a 9090 da ConnectionRefused y no es un fallo del payload.
Fue un bloqueo real durante el proyecto. No diagnosticar con connect-and-close (ver sección 8).

### 12.5 API de notificaciones (datos verificados del SDK fijado)
SceNotificationRequest: tamaño total 0xC30, campo message[1024]. El build lo comprueba con
_Static_assert. El símbolo ° en UTF-8 SE RENDERIZA CORRECTAMENTE en pantalla (confirmado por
el usuario). Patrón de mensajes usado, con el valor anterior real leído por GET:
- fan 50-75: "Ventilador ajustado: 60 °C (antes 79 °C)"
- fan 80:    "Ventilador: 80 °C (antes 79 °C). Superior al original"
- default:   "Ventilador: umbral original restaurado (79 °C)"
- disable:   "Actualizaciones bloqueadas. Se mantienen al apagar"
- enable:    "Actualizaciones habilitadas. Pueden descargarse"
- abortos:   "Carga GoldHEN antes de este payload" / "Este GoldHEN no expone el control del
              ventilador" / "No se pudo leer el ventilador. Sin cambios"
Los mensajes de aborto son tan importantes como los de éxito.

### 12.6 Valores de byte 5 por umbral
50=0x32  55=0x37  60=0x3C  65=0x41  70=0x46  75=0x4B  80=0x50  default/stock 79=0x4F

### 12.7 Corpus original: solo referencia, no ejecutar
Los 27 BIN originales (12 de updates para 5.05/6.72/7.02/7.55/9.00/11.00, 7 fan de 5.05,
8 fan de 9.00) están en el proyecto, en carpetas como "Fan 9.00". Son el corpus de análisis.
NO ejecutarlos salvo autorización explícita. Los dos únicos que se usaron en pruebas:
3fce5c84aa16e442f0d1f756d595fc9c773cef588a8f7b26b4934140f8bc69c2  Fan 9.00/fan60.bin
75fd871fc09c49a18b8006b8120a5e2e407196f6cb03289b949fd50419bdeffc  Fan 9.00/fanDefault.bin
Los fan de 5.05 coinciden por SHA-256 con el repo público PS4-fan-control-payloads.

### 12.8 URLs de referencia
Repo de referencia (mismos bugs de buffer, sin GET, umbrales 60/70/80/85):
https://github.com/WPA-Libertario/PS4-Payloads-4.50-13.52
Upstream del cargador ELF (resolución de símbolos en runtime, SPRX):
https://github.com/ps4-payload-dev/elfldr
Análisis ya hecho: analysis/REFERENCE_REPO_FINDINGS.md
Los disable de ese repo, igual que los originales, incluyen un wrapper condicional de syscall 11:
su rango amplio no es userland puro. Nuestra arquitectura sí lo es.
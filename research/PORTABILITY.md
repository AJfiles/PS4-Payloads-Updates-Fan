# Hallazgos de portabilidad GoldHEN (P1–P5)

## Estado de hardware A, B, D, E, C, T50…TDEFAULT, F0 y F_RECHECK — 2026-11-20

El usuario ejecutó las medidas y reportó la versión GoldHEN `2.4b18.10` leída en el menú. Esta evidencia supersede los estados `UNKNOWN/no probado` de las secciones históricas **solo para PS4 Pro 9.00 + GoldHEN 2.4b18.10**. No se extrapola a otros modelos o firmwares.

| Componente | Resultado reportado en hardware | Estado |
|---|---|---|
| BinLoader / precondición | Puerto 9090; UID 0, GID 0, sandbox 0; precondition PASS | **VERIFICADO en esta consola** |
| `/dev/icc_fan` | Existe; character device, modo 8630 decimal (`0666`), size 0; open fd 20 | **VERIFICADO en esta consola** |
| ICC GET `0xC01C8F08` | Dos respuestas rc 0/errno 0 e idénticas, 28 bytes | **VERIFICADO en esta consola** |
| Tabla stock | `00 00 00 00 00 4f 00 00 00 08 00 00 80 00 00 00 ff ff ff 0f ff ff ff 0f ff ff ff 0f` | **VERIFICADO en esta consola** |
| Firmware | `sceKernelGetSystemSwVersion=" 9.008.031"`, numeric `0x09008031`; sysctl raw `31 80 00 09` | **VERIFICADO; ambas vías concuerdan** |
| Rutas PUP | Ambas son directorios, modo 16895 decimal (`0777`), size 32768; bloqueo ya aplicado | **VERIFICADO en esta captura** |
| B, fan60 original | Umbral `0x4f → 0x3c`; solo cambió byte 5; aceleración audible y notificación reportadas por el usuario | **SET original validado end-to-end en esta consola** |
| D, default original 79 | Umbral `0x3c → 0x4f`; solo cambió byte 5; tabla final igual a A | **VERIFICADO por reporte recibido** |
| Estado ICC actual tras D | Byte 5 = `0x4f` (79) | **Reportado por la medida D** |
| E, fan60 universal | Primer intento revirtió exactamente a 79 por bug del verificador. E2 corrigió el comparador: `fan.status=VERIFIED`, GET/SET rc 0; solo byte 5 `0x4f→0x3c`; sonda posterior estable dos veces y 0/28 diferencia contra B. Hash `7ee35577a903899037630ad5d201dc9a03aa480dbf9b757753186375ce702ad0`; usuario reportó notificación visible, ° correcto y aumento audible del ventilador. | **VERIFICADO PS4 Pro 9.00 + GoldHEN 2.4b18.10** |
| GoldHEN | Usuario confirmó versión `2.4b18.10` en el menú | Build exacto reportado por el usuario | **VERIFICADO como contexto de A/B/D/E2/C en PS4 Pro 9.00; no implica versión mínima** |
| C, persistencia fan | Tras reinicio/recarga, GET dos veces estable en byte 5=`0x3c`; otros 27 valores iguales a E2 | Estado fan persiste a reinicio en esta consola | **VERIFICADO** |
| C, rutas update | Ambas permanecen directorios con permisos `0777` | El bloqueo sigue aplicado tras reinicio | **VERIFICADO en esa captura** |
| Umbrales fan universales | 50/55/60/65/70/75/80/default 79 medidos individualmente; cada tabla solo difirió en byte 5 y GET confirmó el resultado | Estado final 79; 80 mostró advertencia por superar el stock observado | **VERIFICADO en PS4 Pro 9.00 + GoldHEN 2.4b18.10** |
| F0, intento 4 (histórico) | Ambas rutas directorios root-owned `0555`, vacíos (`getdents` count 0); FTP EPSV/PASV coincide | Disable de esa build informó `DISABLED` y preservó ambas rutas. | **F0 VERIFICADO como idempotente en ese estado** |
| F_RECHECK | Preflight: ambas rutas exactas directorios vacíos `0777`; reporte en [probe_F_recheck.txt](../analysis/probe_F_recheck.txt) | BIN nuevos: enable hizo `rmdir` rc 0 en ambas sin filtrar owner/modo; sonda confirmó ausencia; disable recreó ambas y sonda confirmó directorios vacíos. Operador confirmó `noupdt` y descargas automáticas apagadas. | **VERIFICADO en esta consola para los BIN reconstruidos** |
| Versión GoldHEN | La sonda informa UNKNOWN (no consulta API privada); usuario confirmó `2.4b18.10` leyendo el menú | **Build verificado en estas medidas; versión mínima desconocida** |

La versión mínima de GoldHEN que garantiza `/dev/icc_fan` sigue sin determinarse. La evidencia fija únicamente 2.4b18.10 como un build en el que nodo, GET y contexto root sin sandbox se verificaron en PS4 Pro 9.00. C registró GET=`0x3C` tras el reinicio reportado y antes de los smoke tests posteriores; la secuencia detallada está en [C_AND_F_AUDIT.md](../analysis/C_AND_F_AUDIT.md). Temperaturas de juego por encima de 75 °C no invalidan por sí mismas la persistencia del byte: es un objetivo de ventilador, no una garantía de temperatura máxima.

La comparación A→B muestra que el fan60 original cambió solo byte 5 en el estado leído después del SET; offsets 9, 12 y 16–27 permanecieron iguales a stock. Así, el kernel ignoró esos campos de entrada para el efecto observable en esta consola y el original fue funcionalmente equivalente en el umbral. La razón para reescribirlo es el riesgo de memoria: `_IOWR` copia 28 bytes desde un objeto de pila de 10 bytes y vuelve a copiar 28 bytes sobre la pila. El payload nuevo hace RMW de 28 bytes, cambia solo byte 5, verifica los otros 27 y aborta sin SET si el GET falla. Eso evita el acceso fuera de los límites del objeto y preserva campos por compatibilidad futura; no es evidencia de que el kernel 9.00 los use.

La captura ampliada posterior [probe_F_idempotence2.txt](../analysis/probe_F_idempotence2.txt) confirma ambas rutas con `entries.count=0`, `entries.status=OK`, UID 0 y permisos `0555`. El byte ICC leído en esa misma captura fue 79, después del smoke test de default. El usuario confirma además que el fan60 original produjo una notificación y aceleración audible; el log estructurado B conserva el diff `0x4f→0x3c`, sin log TCP original.

El resto de este documento conserva el análisis estático de corte 2026-11-18 salvo las correcciones explícitas de buffer/observación, las filas de hardware aquí resumidas y las pruebas posteriores. GoldHEN `2.4b18.10` fue confirmado por el usuario en el menú; versión verificada, no mínima. F0 histórico se conserva en `probe_F_idempotence4.txt`; el roundtrip de binarios anteriores está en `probe_F.txt`, y F_RECHECK confirma el roundtrip de los binarios reconstruidos.

## Alcance honesto de “universal”

- **VERIFICADO:** PS4 Pro 9.00 + GoldHEN 2.4b18.10; pruebas fan y runtime registradas en esta consola.
- **ALTA CONFIANZA TÉCNICA, NO GARANTIZADO:** otras PS4 con GoldHEN que den UID 0, sandbox inactivo, `/dev/icc_fan`, ioctls `0xC01C8F07/08` y rutas `/update` estándar. Esas son precondiciones runtime; si faltan, el código aborta antes de SET o modificación de rutas.
- **Evidencia adicional de updates:** los seis payloads originales 5.05–11.00 contienen las mismas dos rutas. Esto apoya portabilidad de las rutas, pero no sustituye pruebas de los BIN nuevos por firmware.
- **Evidencia adicional de fan:** los fan originales 5.05 y 9.00 usan `/dev/icc_fan`; el fan nuevo solo está verificado en PS4 Pro 9.00.
- **DESCONOCIDO/EXPERIMENTAL:** GoldHEN antiguo, loaders distintos, builds comunitarios, 14.02/Afterglow y modelos no probados.

No hay offsets de kernel en los nuevos BIN. “Universal” significa universal por diseño condicionado a interfaces GoldHEN verificadas; no es garantía absoluta ni porcentaje de compatibilidad.

## P1 — ¿Hay dependencia real de firmware?

**En los BIN originales: sí. En la ejecución userland desde BinLoader de esta PS4 Pro 9.00 + GoldHEN 2.4b18.10: no; la medida A reportada por el usuario verificó UID 0 y sandbox desactivado.** Ese resultado solo cubre esta combinación de hardware y build.

En el SDK fijado `Scene-Collective/ps4-payload-sdk` (`fd4f7299d1da5f5601c164a02b6d6d019ac8630a`), `is_jailbroken()` comprueba UID 0 y que `is_in_sandbox()` sea falso. `jailbreak()` retorna inmediatamente si ambas condiciones se cumplen; de lo contrario prepara `fw_version` y llama a `kexec(&kpayload_jailbreak, ...)`. El kpayload modifica credenciales/prison/root vnode y usa macros con offsets por firmware. Por ejemplo, el SDK consultado contiene offsets distintos de `PRISON_0` para 5.05 y 9.00. La syscall `kexec` se invoca por syscall 11. Es una ruta de escalada/escape dependiente del firmware, no simple ruido.

El flujo observado de los payloads fan llama a `jailbreak()` antes de abrir `/dev/icc_fan` y efectuar el ioctl. El BIN por tanto incluye código de usuario y fallback privilegiado; las instrucciones privilegiadas y los offsets del fallback no prueban que se ejecuten al cargarlo bajo GoldHEN. La documentación oficial de GoldHEN publica BinLoader en 9090, pero lo llama experimental y no especifica UID/sandbox del contexto de ejecución. La medida A reportada por el usuario confirmó UID 0 y sandbox desactivado en GoldHEN 2.4b18.10 sobre esta PS4 Pro 9.00. No se infiere comportamiento de otros builds.

**Conclusión operativa:** `DEPENDENCIA DEL BIN LEGACY: SÍ; DEPENDENCIA DEL NUEVO USERLAND EN ESTA PS4: NO, VERIFICADO EN A`. La guarda runtime continúa obligatoria para otros BinLoader/builds: UID distinto de 0 o sandbox activo aborta sin tocar estado. `mkdir/rmdir` y `open/ioctl` no necesitan offsets; dependen de permisos/rutas/nodo.

### Diferencias 5.05 vs 9.00

Son artefactos de tamaños distintos (fan 5.05: 4,880 bytes; fan 9.00: 7,472 bytes). En el grupo 5.05, el parámetro cambia en `0x173` y además el texto de umbral varía en `0xCE5–0xCE6`; en 9.00 el byte de parámetro está en `0x1D28`. El fan50 de ambos builds comparte 2,314 bytes en bloques alineados por comparación de secuencia (37.47% sobre el total comparado); esto es una métrica de similitud, no clasificación semántica.

| Clase solicitada | Hallazgo | Cuantificación y límite |
|---|---|---|
| (1) Ruido toolchain/SDK | Explica plausiblemente gran parte de la diferencia estructural; las rutinas de soporte/SDK no son iguales entre esos binarios. | No se puede cuantificar cada byte: no se identificó el toolchain original ni se reprodujo la compilación. El intento con Clang 18 no fue comparable bit a bit. |
| (2) Strings | El texto que representa el umbral cambia junto al dato en 5.05. | Dos bytes de texto observados en `0xCE5–0xCE6` (además del byte de dato `0x173`). La disposición 9.00 usa otra región. |
| (3) Parámetro | El umbral está codificado como dato inmediato/byte en offsets distintos según build. | Una posición conocida por bin en cada grupo: `0x173` (5.05), `0x1D28` (9.00). Valor 50 (`0x32`) en fan50. |
| (4) Dependencia real de firmware | `jailbreak()` fallback invoca `kexec` y construye kpayload usando offsets específicos del firmware. | Confirmada por el flujo de fuente del SDK. No se demuestra que esa rama se ejecute al cargar desde BinLoader de GoldHEN. |

**No clasificado:** no atribuyo todas las regiones restantes a (1) porque faltan toolchain, commit exacto de SDK original y un mapeo de funciones comparable. Diferencias sin atribución están en el informe de diff/desensamblado. La comparación 5.05↔9.00 no permite llamar a cada diferencia “dependencia de firmware”; la única dependencia real confirmada por fuente es el fallback de jailbreak.

## P2 — Reproducción y hashes

La reconstrucción se intentó desde el source público del fan conocido. Herramientas: Zig 0.13.0 con Clang 18.1.6 y SDK checkout fijado. La compilación local produjo 9,797 bytes (`3b891c3dd2b4a90bf4ddab3594c75ecff3ba6218a8ae21ddd0028d958c2ab14d`); el fan60 original mide 4,880 bytes y su SHA-256 es `bbc109ed6f6dcf5def9ad64b1cdf2de3f4756edecefe351096cfd7fddb5fa2b2`. Alineación por secuencia: 593 bytes comunes (8.08%). Las adaptaciones de sintaxis se hicieron únicamente en las copias de `work/rebuild-fan-5.05`; no se tocó la fuente original ni el BIN original.

**Resultado: no reproducido bit a bit.** No hay base para separar con rigor cuánto se debe a timestamp, rutas, linker/padding, compilador o a diferencias entre el SDK clonado y el SDK con que se produjo el BIN histórico. La compilación no valida por sí sola funcionamiento en hardware.

Hashes exactos buscados en repositorios públicos clonados y por consultas de hash: 7/27 sí encontrados, todos los fan 5.05 en `toxxic407/PS4-fan-control-payloads`; los otros 20 (12 updates y 8 fan 9.00) no encontrados. No se afirma que no existan en ninguna parte, solo que no aparecieron en las fuentes consultadas.

## P3 — Contrato del control de ventilador

### Codificación y buffer

El valor `0xC01C8F07` decodifica como `_IOWR`, grupo `0x8F`, número `7`, tamaño `0x1C` = 28 bytes. El fuente original pasa solo 10 bytes `{00,00,00,00,00,T,00,00,00,00}`. Aunque esos bytes no representan los valores stock observados en offsets 9, 12 y 16–27, la lectura B después del SET original mostró esos 27 campos sin cambio. Por ello, en esta PS4 9.00 los campos de entrada adicionales no alteraron el efecto visible en el perfil. El problema confirmado es de seguridad de memoria: el kernel copia 28 bytes desde y hacia un objeto de pila de 10 bytes, leyendo y escribiendo 18 bytes fuera de sus límites. La implementación nueva hace RMW de los 28 bytes, cambia solo byte 5 y aborta sin SET si falla el GET inicial.

Una fuente PS4 independiente, WebRTE (`saudid0s/WebRTE`, commit `557eea8141d6991d848366fd103dc635936285fd`), trata la estructura como 28 bytes: bytes 0–1 estado, byte 2 selector de zona, byte 3 flag/auxiliar, luego seis `uint32_t` en offsets 4–27. Su código usa `0xC01C8F07` para SET y `0xC01C8F08` para GET. El resultado B es evidencia adicional en PS4 9.00: al enviar el objeto corto original, solo cambió el umbral leído después; los otros campos de salida permanecieron iguales. Esto indica que el kernel ignoró esos campos de entrada en ese caso, pero **no prueba el ABI de cada firmware/modelo ni explica definitivamente cada dword**. El payload nuevo conserva la estructura real completa y no depende de esa inferencia.

`0xC01C8F08` está definido como `_IOWR`, grupo `0x8F`, núm. 8, tamaño 28. WebRTE lo usa como GET. La medida A reportó rc/errno 0 y dos lecturas idénticas de 28 bytes en esta PS4 Pro 9.00, por tanto GET queda **VERIFICADO en esta consola**. No se extrapola a otros firmware/modelos. La fuente enumera además GET medido/error `0xC0148F09`, pareja set/get byte `0xC0048F03/04` y set/get duty `0xC0068F05/06`; no se usan en los nuevos BIN.

### Valores, rango, efectos y persistencia

El byte 5 fue identificado como umbral por los payloads y la tabla stock A reportó 79 (`0x4f`) en esa posición. El preset default histórico de 9.00 aplica 79; no restaura una tabla previa. En esta consola coincide con el byte stock observado, pero semánticamente sigue siendo una aplicación explícita de 79.

WebRTE describe una tabla de seis dwords por zona y publica valores de fábrica para su contexto 13.00. Identifica los tres primeros como setpoint/ganancias en su implementación y los tres últimos como parámetros adicionales/sentinel; no usar ese comentario para certificar todas las versiones. Su README advierte que tablas inválidas/cero pueden saturar el ventilador a velocidad alta en el firmware probado. No se encontró especificación PS4 oficial que fije rango universal o comportamiento fuera de rango. No afirmar que el umbral desactiva protecciones térmicas; tampoco se puede garantizar lo contrario solo desde estos payloads. El mecanismo modifica el objetivo/control de ventilador, no hay evidencia de que parchee thermal shutdown.

El código original hace open → ioctl → close → retorno: fire-and-forget, no residente. El ioctl se reenvía al servicio ICC/SYSCON según el código WebRTE. En PS4 Pro 9.00 + GoldHEN 2.4b18.10, la medida C confirmó que 60 sobrevive a reinicio y recarga de GoldHEN; aplicar 79/default sirve para volver al umbral stock observado. Esto no demuestra persistencia en otros firmware/modelos ni qué ocurre al apagar completamente. El perfil térmico puede diferir entre hardware; no se ha comparado FAT/Slim/Pro.

**Default y restauración:** 79 es el valor stock observado en esta consola; el BIN `fan_default` aun así aplica 79 y no restaura un snapshot anterior. Para reversibilidad exacta hace falta guardar la tabla previa y reponerla íntegra.

## P4 — Bloqueo de actualizaciones

Los seis pares (5.05, 6.72, 7.02, 7.55, 9.00, 11.00) contienen las mismas rutas literales:

- `/update/PS4UPDATE.PUP.net.temp`
- `/update/PS4UPDATE.PUP`

El disable intenta retirar entradas en esas rutas y crear directorios señuelo; enable intenta unlink/rmdir. El source de Scene-Collective confirma intención de bloquear la descarga de PUP de sistema al ocupar las rutas con directorios. No se demuestra que bloquee parches de juegos o DLC; no presentar ese efecto como garantizado. La modificación vive en el filesystem, por lo que sobrevive a un apagado/reinicio normal, salvo limpieza del sistema (el README fuente advierte cleanup en ciertos escenarios de kernel panic entre FW 5.05–6.72). No equivale a estado persistente irreversible.

El source histórico `enable` no ofrecía salvaguardas suficientes. En los candidatos nuevos, enable hace preflight de ambas rutas, rechaza archivos, tipos no esperados y directorios no vacíos, y solo usa `rmdir` sobre las dos rutas canónicas cuando getdents confirma que están vacías, sin exigir UID ni modo; `getdents` confirma que están vacíos. No hay marcador durable que pruebe qué programa creó una carpeta idéntica; el criterio implementado es ruta canónica + directorio + vacío. Nunca usa unlink ni borrado recursivo.

Disable con los dos directorios señuelo vacíos es idempotente; enable con ambas rutas ausentes también es no-op. Disable sobre un archivo regular informa el tamaño y lo elimina antes de crear el directorio: el PUP/contenido se pierde. El usuario autorizó expresamente esa conducta para disable; los tests de updates deben permanecer sin Internet.

## P5 — Disponibilidad y evidencia GoldHEN

Los binarios de releases se escanearon como datos; no se ejecutaron. El conjunto adquirido comprende tags 2.2, 2.2.1–2.2.4, 2.3, 2.4b17, 2.4b17.2, 2.4b17.3 y 2.4b18; no es una afirmación de cobertura de cada beta histórica `2.4b*`. Se buscaron literales ASCII y UTF-16LE para `/dev/icc_fan`, codificación de ioctl, rutas update, “BinLoader” y “9090”. La ausencia de strings no demuestra que falten el servicio o código: puede resolverse dinámicamente o estar eliminado. La documentación oficial de GoldHEN indica BinLoader 9090 pero lo marca experimental; no publica el contexto de credenciales. GoldHEN no parece “crear” el nodo ICC: la evidencia de WebRTE lo trata como char device del sistema/controlador kernel. Esa fuente de terceros no demuestra disponibilidad en toda versión.

| Build | Firmware declarado en release/artefacto | `/dev/icc_fan` en strings | BinLoader | Evidencia / conclusión |
|---|---|---|---|---|
| GoldHEN 2.2, 2.2.1, 2.2.2 | Assets nombran 5.05, 6.71, 6.72, 7.02, 7.50, 7.51, 7.55, 9.00 (cada release puede variar; manifiesto es fuente exacta) | No literal | Release notes/documentación de esas releases indican BinLoader 9090; sin credenciales | UNKNOWN nodo/contexto. |
| GoldHEN 2.2.3, 2.2.4, 2.3 | Assets nombran 5.05, 6.72, 9.00 | No literal | 9090 documentado; BinLoader advertido como experimental en releases | UNKNOWN nodo/contexto. |
| GoldHEN 2.4b17 | 5.05, 6.71/6.72, 9.00, 11.00 | No literal | 9090 documentado como experimental | UNKNOWN nodo/contexto. |
| GoldHEN 2.4b17.2 | 5.05, 6.71/6.72, 9.00, 10.00/10.01, 11.00 | No literal | 9090 documentado como experimental | UNKNOWN nodo/contexto. |
| GoldHEN 2.4b17.3 | 5.05, 6.71/6.72, 9.00, 9.60, 10.00/10.01, 11.00 | No literal | 9090 documentado como experimental | UNKNOWN nodo/contexto. |
| GoldHEN upstream 2.4b18 | 5.05, 6.71/6.72, 9.00, 9.60, 10.00/10.01, 10.50, 10.70/10.71 y 11.00 según release oficial | No literal | 9090 documentado como experimental | UNKNOWN para contexto exacto; upstream no declara 14.02. |
| GoldHEN 2.4b18.10, confirmado en el menú | 9.00 | Presente y accesible; GET y fan60 universal E2 probados | BinLoader 9090 ejecutó sonda y payloads según reportes | VERIFICADO en PS4 Pro para contexto y fan60; otros modelos/builds EXPERIMENTAL. No fija versión mínima. |
| GoldHEN incluido en Afterglow (SHA-256 `c5480587d99efc2e3cc132352d9b7cf052dd3068280f1686e7fbb37d57e9ba8b`) | Afterglow declara FW hasta 14.02 | No literal en strings | El código host inyecta BinLoader en 9090; su helper propio también reconoce payload raw (`e9` + salto) y lo mapea/inicia en thread | `afterglow.js` identifica “GoldHEN v2.4b18.12 loaded”; build comunitario incluido con permiso del autor, no upstream release ni fork formal documentado. Binario distinto del upstream 2.4b18. No está demostrado que la inyección exponga el mismo ABI/contexto que BinLoader upstream. Nodo ICC UNKNOWN. |

**Quién proporciona el nodo:** evidencia del código WebRTE apunta a un dispositivo ICC del kernel/sistema y a llamadas encaminadas a SYSCON; GoldHEN provee jailbreak/servicios de carga, pero no se encontró evidencia de que registre `/dev/icc_fan`. No hay prueba de prerequisito de “cargar un fan payload” para crear el nodo. Disponibilidad exacta por firmware/build/modelo queda UNKNOWN.

## Matriz de cobertura GoldHEN por firmware y función

`Sí*` significa loader descrito por GoldHEN general, no credenciales probadas. `Nodo?` significa que ninguna fuente consultada confirma su presencia en ese build concreto. El campo de kernel queda cancelado/no aplicable a la arquitectura userland prevista.

| Build / FW | Firmware | ICC | Loader payload | Funciones posibles | Estado / riesgo |
|---|---|---|---|---|---|
| Upstream 2.2/2.2.1/2.2.2 | 5.05, 6.71, 6.72, 7.02, 7.50, 7.51, 7.55, 9.00 según nombres de assets | Nodo? | Sí* | Updates userland; fan condicionado por ICC | EXPERIMENTAL; falta confirmar permisos/ABI |
| Upstream 2.2.3/2.2.4/2.3 | 5.05, 6.72, 9.00 según nombres de assets | Nodo? | Sí* | Igual | EXPERIMENTAL |
| Upstream 2.4b17–2.4b18 | Firmware discreto según notas de cada release, detallado en la tabla superior | Nodo? | Sí* | Igual | EXPERIMENTAL; upstream no declara 14.02 |
| GoldHEN 2.4b18.10, menú confirmado | 9.00 | Presente y accesible; GET probado; fan60 universal E2 probado | BinLoader 9090 ejecutó sonda y payloads según reportes | VERIFICADO para PS4 Pro 9.00 + build 2.4b18.10; otros builds EXPERIMENTAL |
| Afterglow GoldHEN v2.4b18.12 (etiqueta embebida) | 12.00–14.02 según host comunitario | Nodo? | Host aplica patchsets, inyecta BinLoader 9090; helper separado carga raw por JMP rel32 en thread | Solo si API ICC existe y permisos/ABI valen | EXPERIMENTAL; build distinto/no upstream; credenciales/ABI del loader no documentados |

No se incluyen otros firmware por interpolación. La cobertura confirmada en hardware es PS4 Pro 9.00 + GoldHEN 2.4b18.10: A verificó contexto/nodo/GET; B y D los originales; E2 y los smoke tests verificaron el control fan; C observó byte 5=`0x3C` después del reinicio reportado; F_RECHECK validó enable→disable con los BIN reconstruidos. F0 histórico probó preservación idempotente de directorios vacíos `0555`. La versión mínima sigue UNKNOWN; los demás builds/modelos y updates siguen sin validación.

## Tabla de fuentes principales

- [GoldHEN releases oficiales](https://github.com/GoldHEN/GoldHEN/releases): bins, notas y soporte declarado; no especifica credenciales del hilo BinLoader.
- [ps4-payload-sdk fijado](https://github.com/Scene-Collective/ps4-payload-sdk/tree/fd4f7299d1da5f5601c164a02b6d6d019ac8630a): `is_jailbroken`, `jailbreak`, `kexec`, macros/offsets.
- [WebRTE PS4](https://github.com/saudid0s/WebRTE): tabla ICC SET/GET y código de lectura/verificación; evidencia de tercero para contexto FW declarado por el repo.
- [Tabla PS4 ioctl](https://www.psdevwiki.com/ps4/IOCTL): referencia comunitaria para código/número ioctl.
- [Fuentes Scene-Collective](README.md): disable/enable updates y fan threshold.
- [Afterglow web host](https://github.com/well0nez/afterglow-web): cadena 14.02 comunitaria/host y artefacto GoldHEN incluido.

# Hallazgos del repositorio de referencia WPA-Libertario

Fuente fijada: [`WPA-Libertario/PS4-Payloads-4.50-13.52`](https://github.com/WPA-Libertario/PS4-Payloads-4.50-13.52/tree/a5e483abc5011dff2d72b1fd4bfe5dc5bdf57f74), commit `a5e483abc5011dff2d72b1fd4bfe5dc5bdf57f74`. Descarga limitada a los tres payloads de updates solicitados, carpeta `fan-thresholds` completa, `elfldr.elf` y `elfldr-4-50-5-07_7-00-9-60.bin`; ningún artefacto fue ejecutado.

## Archivos y hashes

| Archivo | Bytes | SHA-256 |
|---|---:|---|
| `ps4-disable-updates-4-73-13-52.bin` | 8764 | `325b570a7626c5073f39c82a920315b2dfeb6c9154735609118f2b0af0052794` |
| `ps4-enable-updates-4-73-13-52.bin` | 8732 | `86f45dca323e9ab53aecb1cfb715322a0ffd70582564ad34c593beaaed6d422c` |
| `ps4-disable-updates-4-50-5-07.bin` | 5984 | `d29b66e9396281e4596e8cfa68c9ce0c252b02342d7710697edf60688262ccfa` |
| `fan-thresholds/ps4-fan-threshold60.bin` | 8992 | `73bda82299ce84440c678d0b20704bf74df507897eeffb392e7fb076f4417b13` |
| `fan-thresholds/ps4-fan-threshold70.bin` | 8992 | `7be7b600f0ef89aa5bfa5e7f40d549103138286c0c04d0004f7bc9a6f6d0176c` |
| `fan-thresholds/ps4-fan-threshold80.bin` | 8992 | `8734cbcd0ffaf2fe1f3169f201dc6d03d2674dd2f9f4c9e256e17945572835dd` |
| `fan-thresholds/ps4-fan-threshold85.bin` | 8992 | `4793641950377600897cd799d9da1365dfe60452c3de93ed815f104fafc576ec` |
| `fan-thresholds/README.md` | 2358 | `c17bea8cbc535b0071b285383dfe898057f6f0bce89bfe170d9fdacf98af7f51` |
| `elfldr.elf` | 263360 | `fa39e1fb1df62f23703830ed2e2c18cba8ee7582247b09b386851360fe37f8c4` |
| `elfldr-4-50-5-07_7-00-9-60.bin` | 265748 | `a70c77c644a5f6aa1535515ed5a3354061870b49727bf2581264e6f8ea2c8398` |

El directorio fan contiene exactamente el README y cuatro BIN: 60, 70, 80 y 85. No hay fuente C junto a esos artefactos en el checkout fijado. El README general afirma rangos de compatibilidad hasta 13.52; son declaraciones del autor, no validación independiente por cada firmware o modelo.

## Updates: qué hace un BIN de rango amplio

Los tres BIN son raw x86-64. Las dos rutas literales observadas en disable/enable son `/update/PS4UPDATE.PUP.net.temp` y `/update/PS4UPDATE.PUP`; las cadenas de log son `Disabled updates!` y `Enabled updates!`. Disable incluye llamadas a syscall 136 (mkdir) desde las rutas, con `esi=0x309` en offsets `0x53` y `0x7C` del BIN 4.73–13.52. `0x309` es 777 decimal, que como `mode_t` equivale a octal `01411`, no a `0777`; el modo final también depende del `umask`. El disable 4.50–5.07 pasa el mismo argumento en offsets `0x3E` y `0x67`. En enable no aparece ese argumento de modo; las rutas y los wrappers unlink/rmdir están presentes. La tabla completa de strings y disassembly está en [`REFERENCE_PAYLOADS.md`](REFERENCE_PAYLOADS.md) y [`disasm/reference/`](disasm/reference/).

El desensamblado de los seis disable originales también muestra `esi=0x309` antes de llamar al wrapper que carga `rax=0x88` (syscall 136): 5.05 en `0x3E/0x67`, 6.72 en `0x6D4/0x6FD`, 7.02 en `0x12DE/0x1307`, 7.55 en `0x51/0x7A`, 9.00 en `0x4C/0x75` y 11.00 en `0x51/0x75`. Es el mismo argumento decimal 777; no se debe describir como modo octal 0777 ni como 0555. No se infiere intención del autor, y el modo efectivo depende del `umask` de ejecución.

No aparece una tabla textual de versiones en el BIN disable. La entrada hace una llamada a una rutina de comprobación; en el camino de fallo entra a una rutina de preparación y luego llama al wrapper syscall 11 (`kexec`) desde `0x1829` (el wrapper es `0x32D`). Así que el artefacto incluye físicamente un fallback de ejecución privilegiada; no es solo un BIN de `mkdir`. La rutina de comprobación llama a través de un puntero resuelto en runtime y al wrapper 585, pero no hay símbolos de depuración ni fuente coincidente para identificar de forma concluyente cada indirect call. No encontramos una tabla literal de versiones u offsets en los strings; el README declara 4.73–13.52, pero el artefacto solo no demuestra qué mecanismo da ese rango ni valida cada firmware. La operación de filesystem usa los mismos nombres/rutas, así que esa parte en sí no requiere offsets de kernel; la portabilidad del fallback es una cuestión separada.

Respecto a nuestro payload, la ventaja es el alcance declarado y el formato ya empaquetado para un único rango amplio. Nuestro diseño agrega preflight de ambas rutas, comprobación de directorios vacíos, rechazo de tipos inseguros, rollback limitado y mensajes explícitos. También usa modo octal `0777` con `umask(0)`, a diferencia del argumento `0x309` observado en este artefacto. El BIN universal actual no lleva syscall 11 ni fallback kexec: requiere que la comprobación de UID 0 y sandbox pase y sale limpio si no.

## Fan: contrato observado

Los cuatro BIN de fan tienen el mismo tamaño y el mismo flujo de código. Usan `/dev/icc_fan` y SET ioctl `0xC01C8F07`; no aparece GET `0xC01C8F08`. La tabla de datos se inicializa con `rep stosb` para **10 bytes**, luego se escribe el umbral en `[buffer+5]` y se hace ioctl. Por tanto, reproducen el buffer subdimensionado de 10 bytes, no el buffer de 28 bytes, y no conservan campos leídos previamente. Los umbrales ofrecidos son 60/70/80/85. El README declara compatibilidad 4.73–13.52, pero no prueba esos BIN en todas las versiones o modelos.

## Modos 0555 y 0777

La observación del PS4 mostró que ambos señuelos han existido como directorios vacíos root-owned tanto `0555` como `0777`. El modo del hijo no vuelve a `rmdir` resistente: FreeBSD exige que el directorio esté vacío, y comprueba permisos de escritura en el directorio padre; no requiere permisos sobre el directorio que se elimina. El `/update` observado es `0777`, así que `0555` no bloquea por sí mismo el borrado del nombre por un proceso con acceso al padre. `0555` sí dificulta crear contenido dentro para procesos no privilegiados, pero un updater privilegiado puede superar esa restricción y no existe evidencia aquí de la identidad del proceso de actualización.

La discrepancia del modo queda **NO RESUELTA**. Los seis disable originales y el artefacto de referencia pasan `0x309` (decimal 777, octal `01411`) a `mkdir`; `umask` solo puede quitar bits, por lo que no explica los modos observados `0777` y `0555`. Las posibilidades pendientes son un `chmod` posterior, normalización del filesystem o que los directorios observados los haya creado otro payload. No se atribuye el modo observado a esos `mkdir` sin evidencia adicional.

El modo no determina si el señuelo bloquea el update: lo que importa es que la ruta exacta exista como directorio en lugar de fichero PUP. El `enable` reconstruido acepta ruta canónica + directorio + vacío, independientemente de modo y propietario; el criterio de eliminación de `rmdir` se basa en que el directorio esté vacío y en permisos sobre el padre. La hipótesis de que `0555` impide `rmdir` queda descartada por [FreeBSD `rmdir(2)`](https://man.freebsd.org/cgi/man.cgi?query=rmdir&sektion=2&manpath=FreeBSD+14.0-RELEASE). La compilación de los dos BIN corregidos está registrada en el manifiesto; el roundtrip hardware de revalidación queda pendiente hasta ejecutarse.

Base de permisos: [FreeBSD `rmdir(2)`](https://man.freebsd.org/cgi/man.cgi?query=rmdir&sektion=2&manpath=FreeBSD+14.0-RELEASE), [FreeBSD `mkdir(2)`](https://man.freebsd.org/cgi/man.cgi?query=mkdir&sektion=2&manpath=FreeBSD+14.0-RELEASE).

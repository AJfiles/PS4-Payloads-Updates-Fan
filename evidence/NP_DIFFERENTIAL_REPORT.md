# Informe de Verificación Diferencial de Hardware: NP Environment (Fase 2b)

**Fecha:** 2026-11-22  
**Consola:** PS4 Pro (CUH-7xxx), Firmware 9.00 (`0x09008031`), GoldHEN 2.4b18.10  
**Objetivo:** Verificar empíricamente mediante hardware si la clave candidata `0x0780B007` (`125874183`) y sus slots de cuenta corresponden al ajuste del menú `Settings > ★ Debug Settings > PlayStation Network > NP Environment`.  
**Condición:** SOLO LECTURA estricta vía `sceRegMgrGetStr`. Cero escrituras.

---

## 1. Mediciones de Hardware Ejecutadas

| Medida | Estado de la Consola | Archivo de Evidencia | Clave Slot 1 (`0x0780b007`) | Clave Slot 2 (`0x0781b007`) | Slots 3 y 4 |
|---|---|---|---|---|---|
| **Línea Base** | Menú con `NP Environment = np` | `analysis/probe_np_baseline.txt` | `"np"` (`rc=0`) | `"np"` (`rc=0`) | `""` (`rc=0`) |
| **Relectura en Caliente** | Menú cambiado a `noupdt` (sin reiniciar) | `analysis/probe_np_noupdt.txt` | `"np"` (`rc=0`) | `"np"` (`rc=0`) | `""` (`rc=0`) |
| **Relectura Post-Reinicio** | Consola reiniciada, menú confirma `noupdt` | `analysis/probe_np_post_reboot.txt` | `"np"` (`rc=0`) | `"np"` (`rc=0`) | `""` (`rc=0`) |

---

## 2. Datos Crudos de Registro por Medida

### Slot 1 (`0x0780B007` / 125874183)
- Retorno API: `rc = 0` (SCE_OK), `errno = 0` en las 3 medidas.
- Cadena: `"np"` constante.
- Hex (18 bytes): `6e 70 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`.

### Slot 2 (`0x0781B007` / 125939719)
- Retorno API: `rc = 0` (SCE_OK), `errno = 0` en las 3 medidas.
- Cadena: `"np"` constante.
- Hex (18 bytes): `6e 70 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00`.

### Slots 3 y 4 (`0x0782B007`, `0x0783B007`)
- Retorno API: `rc = 0` (SCE_OK), `errno = 0` en las 3 medidas.
- Cadena: `""` (buffer nulo, no asignado).

---

## 3. Veredicto Técnico y Ground Truth Establecido

1. **La clave candidata `0x0780B007` NO es el ajuste de `Debug Settings` [VERIFICADO]:**  
   A pesar de que el menú de la consola muestra `noupdt` (persistente tras reinicio), las lecturas de hardware de `0x0780B007` y `0x0781B007` continúan devolviendo `"np"` de forma inalterada.
2. **Naturaleza de la clave `0x0780B007`:**  
   Proviene de `np-account-restore` y corresponde al campo de entorno de las credenciales de cuenta de usuario local (`config.dat` offset `0x177`). El menú de `Debug Settings` modifica el entorno de red **global / del sistema**, no el perfil de cuenta local.
3. **Comportamiento de la API `sceRegMgrGetStr` [VERIFICADO]:**  
   - Carga y resuelve limpiamente en PS4 Pro 9.00 / GoldHEN.
   - Retorna código `0` y cadenas legibles. No es un valor entero (`int`).
4. **Decisión de seguridad:**  
   En cumplimiento de la regla: **"Ninguna escritura de registro hasta confirmar la clave con evidencia de hardware"** y **"Si ninguna clave responde [al cambio], dímelo y buscamos otra vía documental. No adivines escrituras."**, queda terminantemente **PROHIBIDO** emitir payloads de escritura sobre `0x0780B007` para simular el menú Debug Settings, ya que se estaría escribiendo en una clave equivocada (perfil de cuenta).

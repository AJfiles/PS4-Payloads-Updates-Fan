# Informe de Prueba Unitaria: Decisión de Señuelos e Independencia de Modo (0555 vs 0777)

**Fecha:** 2026-11-22  
**Entorno de ejecución:** PC local (sin conexión a consola)  
**Herramienta:** `tools/test_decoy_decision.py`  
**Resultado global:** 71 / 71 pruebas superadas (100% éxito)

---

## 1. Contexto y Objetivo

En la Fase 1, la validación en hardware acreditó que los binarios reconstruidos de `disable_updates` y `enable_updates` operan con éxito sobre directorios en modo `0777`.
El addendum del handover (sección 10) dejó abierta la confirmación formal sobre si un modo restrictivo `0555` (observado históricamente en consolas previas) podría afectar la decisión o requerir comprobaciones especiales.

El objetivo de esta prueba es certificar mediante análisis estático y ejecución de matriz unitaria exhaustiva que la lógica del payload (`payloads/updates_control.c:inspect_route`) es **100% independiente de los bits de permisos (`st_mode & 07777`) y del propietario (`st_uid`)**.

---

## 2. Matriz de Combinaciones Evaluadas

Se probaron sistemáticamente todas las combinaciones entre:
- **Modos de permisos:** `0777`, `0555`, `0755`, `0000`, `0644`, `0700`.
- **Propietarios:** UID `0` (root), UID `1001` (usuario estándar), UID `65534` (nobody).
- **Tipos de archivo / estado de directorio:**
  1. Directorio vacío (`directory_is_empty() == 1`).
  2. Directorio no vacío con contenido (`directory_is_empty() == 0`).
  3. Error de lectura de directorio (`directory_is_empty() == -1`).
  4. Fichero regular (PUP real).
  5. Ruta ausente (`lstat` retorna `ENOENT = 2`).
  6. Error de filesystem en `lstat` (`EPERM = 1`, `EIO = 5`, `EACCES = 13`).
  7. Tipos no soportados / inseguros (enlace simbólico `S_IFLNK`, dispositivo de caracteres `S_IFCHR`, de bloques `S_IFBLK`, socket `S_IFSOCK`, FIFO `S_IFIFO`).

---

## 3. Resultados Detallados

| Categoría | Casos evaluados | Estado de retorno (`RouteState`) | Resultado del Test |
|---|---|---|---|
| **Directorio vacío** (todos los modos / UIDs) | 18 | `ROUTE_EMPTY_DIR` (1) | **PASS** |
| **Directorio con contenido** (todos los modos / UIDs) | 18 | `ROUTE_UNSAFE` (3) | **PASS** |
| **Fichero regular** (todos los modos / UIDs) | 18 | `ROUTE_FILE` (2) | **PASS** |
| **Ruta ausente** (`ENOENT`) | 3 | `ROUTE_MISSING` (0) | **PASS** |
| **Errores de stat** (`EPERM`, `EIO`, `EACCES`) | 3 | `ROUTE_ERROR` (-1) | **PASS** |
| **Tipos inseguros** (symlink, fifo, dev, sock) | 5 | `ROUTE_UNSAFE` (3) | **PASS** |
| **Error lectura dir** (`getdents` falla) | 6 | `ROUTE_ERROR` (-1) | **PASS** |
| **Total** | **71** | — | **71/71 PASS (100%)** |

---

## 4. Inspección de Código Fuente (`payloads/updates_control.c`)

La inspección del código fuente confirma que la decisión de bifurcación no lee ni compara la máscara de permisos:
```c
if (S_ISDIR(st->st_mode)) {
  int empty = directory_is_empty(update_paths[index]);
  return empty == 1 ? ROUTE_EMPTY_DIR : empty == 0 ? ROUTE_UNSAFE : ROUTE_ERROR;
}
if (S_ISREG(st->st_mode)) return ROUTE_FILE;
return ROUTE_UNSAFE;
```

Los campos `st->st_mode & MODE_PERMS` y `st->st_uid` se leen exclusivamente para formatear los mensajes informativos en el buffer de reporte (`report_text`), sin incidir en el flujo de control ni en los códigos de retorno.

Además, según la especificación POSIX y FreeBSD (`rmdir(2)`), la eliminación de un directorio vacío depende exclusivamente de los permisos de escritura del **directorio padre** (`/update/`), no de los permisos del directorio eliminado. Un directorio `0555` vacío es eliminado por `rmdir` exactamente igual que uno `0777`.

---

## 5. Veredicto

- **Hardware:** Modo `0777` verificado en consola.
- **Modo `0555`:** **[VERIFICADO]** por inspección estática y 71/71 pruebas unitarias locales.
- **Conclusión:** El hueco del modo `0555` queda formalmente cerrado como cosmético y funcionalmente neutral para los payloads universales de actualizaciones.

# PS4 Universal Payloads: Fan Control & Updates Blocker

Suite universal de payloads independientes de posición (PIC) para PlayStation 4, compilados para ejecutarse de forma agnóstica entre versiones de firmware sin offsets de kernel fijos. Resuelve de forma definitiva los cuelgues por desbordamiento de pila en el control de ventilador y proporciona protección de triple capa frente a actualizaciones accidentales del sistema.

---

## 1. Catálogo Completo de Payloads

| Payload | Función Principal | Efecto Inmediato | Persistencia tras Reinicio | Nivel de Riesgo | Suma SHA-256 |
|---|---|---|---|---|---|
| `fan_threshold_50_universal.bin` | Control Térmico 50 °C | Eleva RPM al alcanzar 50 °C | Persiste tras reinicio (verificado) | Mínimo (mayor desgaste acústico) | `c09533b4811fd49642a1d6bdea84973da0227aa80c5c3f38f8fdbcaa76fbf549` |
| `fan_threshold_55_universal.bin` | Control Térmico 55 °C | Eleva RPM al alcanzar 55 °C | Persiste tras reinicio (verificado) | Mínimo | `e8763eaae27248138ea1d37234919acaba93db1a13b0b1ee1f089695ae69a54f` |
| `fan_threshold_60_universal.bin` | Control Térmico 60 °C | Eleva RPM al alcanzar 60 °C | Persiste tras reinicio (verificado) | Mínimo (equilibrio recomendado) | `7ee35577a903899037630ad5d201dc9a03aa480dbf9b757753186375ce702ad0` |
| `fan_threshold_65_universal.bin` | Control Térmico 65 °C | Eleva RPM al alcanzar 65 °C | Persiste tras reinicio (verificado) | Mínimo | `6e3d3f0f87e687a2f8f3ab202da3446fdd3a32e40a4784027e75dbd91ec2197a` |
| `fan_threshold_70_universal.bin` | Control Térmico 70 °C | Eleva RPM al alcanzar 70 °C | Persiste tras reinicio (verificado) | Mínimo | `31f8ab3898134dda855e2737c656b387e7738bf8a997f6bc5939c5590827b017` |
| `fan_threshold_75_universal.bin` | Control Térmico 75 °C | Eleva RPM al alcanzar 75 °C | Persiste tras reinicio (verificado) | Mínimo | `76676ff8d86382c95cab5d6ef4d07df25e5fdb79112566246c845c42c6f07df8` |
| `fan_threshold_80_universal.bin` | Control Térmico 80 °C | Eleva RPM al alcanzar 80 °C | Persiste tras reinicio (verificado) | Moderado (superior a stock 79 °C) | `bf31765ed3de71955c5a77aa101cf1118dabd0bb587f3d43c11c35cb397a3e94` |
| `fan_default_universal.bin` | Restablecer Térmica de Fábrica | Restaura umbral stock a 79 °C | Persiste tras reinicio (verificado) | Nulo (perfil original de fábrica) | `d2d26d032bccbc0aa9499ad9ab71549c7569a584264074f4f804c13d148f46b2` |
| `disable_updates_universal.bin` | Bloqueo v1 (Solo Sistema de Archivos) | Crea carpetas señuelo inmutables | Persiste tras reinicio (verificado) | Bajo (PSN permanece accesible) | `0a3ae44cf6a691c17432b734687a5d386bc5561298985e882a92b5f6d381db62` |
| `enable_updates_universal.bin` | Desbloqueo v1 (Sistema de Archivos) | Elimina carpetas señuelo | Persiste tras reinicio (verificado) | Alto si se conecta a Internet | `c7cb5370041a982a54cb1cb2cc1c004d435451f5d0b376d93c372cee47e4541a` |
| `disable_updates_full.bin` | Bloqueo v2 (Triple Capa Completa) | Señuelos + auto_download 0 + noupdt | Persiste tras reinicio (verificado) | Bajo (corta PSN por seguridad) | `d164f3ee4df8e42b67a979f4e5c00101789a842615610273c7046bf9a1ad1043` |
| `enable_updates_full.bin` | Desbloqueo v2 (Triple Capa Completa) | Limpia señuelos + auto_download 1 + np | Persiste tras reinicio (verificado) | Crítico: consola expuesta a update | `714ed6707080f5a70c00d1c2ca8d9b7e8ad2988ac39dfbac6a9123cacbcd6990` |
| `write_verify_noop.bin` | Verificación Segura de Escritura | Escribe valores actuales y los relee | Estado sin cambios (comprobación pura) | Nulo (prueba de validación) | `9fcde094b5c0fa6134a9bee0f1823a4506f9549b55778e892f232a101289c6bb` |
| `np_readonly_dual_probe.bin` | Diagnóstico de Solo Lectura | Inspecciona claves y reporta vía socket | No modifica nada (modo lectura) | Nulo (sonda no intrusiva) | `a8722089a2e79183480af16bbed9989df88e337a63b0311be34d051f49c90b80` |

---

## 2. Compatibilidad y Entorno de Ejecución

- **Verificado Empíricamente:** PS4 Pro (firmware 9.00) con GoldHEN 2.4b18.10. Todos los binarios han sido medidos y comprobados en consola física.
- **Soporte Esperado con Evidencia:**
  - Control de ventilador (5.05 a 11.00): Siempre que el dispositivo `/dev/icc_fan` esté activo en el sistema.
  - Bloqueo v1 (4.73 a 13.52): Rutas de actualización estándar compartidas por toda la arquitectura de la familia Orbis OS.
- **Desconocido:** Firmwares 12.00 a 14.00, y modelos Fat o Slim (lógica idéntica pero sin mediciones de laboratorio en esta serie).
- **Limitación en Versiones Superiores:** El factor limitante para firmwares superiores a 11.00 es la disponibilidad del **cargador de binarios crudos (BinLoader)**. GoldHEN oficial cubre hasta 11.00; herramientas como `elfldr` cargan archivos ELF dinámicos y no binarios planos mapeados directamente en memoria. No se deben inventar porcentajes ni garantizar compatibilidad en firmwares no probados.

### Precondiciones Runtime
1. Acceso como superusuario (`uid == 0`, `gid == 0`) y escape del sandbox del sistema.
2. Servidor de carga de binarios crudos activo (BinLoader en puerto 9090).
3. Acceso a `/dev/icc_fan` y soporte de llamadas dinámicas (`libSceRegMgr.sprx` para la suite v2).

---

## 3. Guía Rápida de Instalación y Uso

1. Asegúrese de tener el entorno GoldHEN cargado en su consola.
2. Active el servidor de inyección:
   - Acceda a `Ajustes` > `★GoldHEN` > `Server Settings`.
   - Active la casilla `Enable BinLoader Server` (puerto estándar `9090`).
3. Envíe el binario deseado desde su ordenador o terminal local:
   ```bash
   python tools/send_payload.py --ip <PS4_IP> --port 9090 payloads/v2/disable_updates_full.bin
   ```
   *(Nota: Copiar los binarios a `/data/GoldHEN/payloads/` figura como método alternativo sin verificar en este hardware; el método probado y verificado empíricamente es la inyección por red vía BinLoader en puerto 9090).*

### Flujo de Uso Recomendado
- **Protección diaria:** Ejecute `disable_updates_full.bin` una sola vez. Configure a continuación su umbral térmico preferido (por ejemplo, `fan_threshold_60_universal.bin`). La protección y el umbral persisten tras el reinicio del sistema.
- **Mantenimiento o actualización voluntaria:** Ejecute `enable_updates_full.bin` únicamente si desea actualizar la consola de forma intencionada o conectarse momentáneamente a los servicios de red. Vuelva a ejecutar `disable_updates_full.bin` inmediatamente después.

---

## 4. Advertencias de Seguridad Críticas

> [!WARNING]
> **Riesgo de actualización con `enable_updates_full`:** Este payload revierte por completo las tres capas defensivas. Si la consola se conecta a Internet en este estado, el sistema iniciará de inmediato la descarga e instalación del software más reciente de Sony, eliminando la posibilidad de ejecutar jailbreak de forma irreversible.

> [!IMPORTANT]
> **Bloqueo de servicios en línea por `noupdt`:** El entorno de red `noupdt` rechaza intencionadamente la conexión con los servidores de autenticación de PSN. La PlayStation Store, el juego online y las actualizaciones de títulos estarán desactivados mientras la protección de la suite v2 permanezca activa.

> [!CAUTION]
> **El umbral de 80 °C no enfría más que el valor de fábrica:** La configuración por defecto observada en el hardware de fábrica es de **79 °C**. Establecer un umbral de 80 °C fija un objetivo más permisivo que el original, por lo que el ventilador girará más lento. Se mantiene en la suite únicamente por paridad con opciones históricas de la scene. Recuerde que el umbral térmico es un objetivo del regulador interno, no una cota física máxima garantizada.

---

## 5. Mejoras Técnicas Frente a Implementaciones Previas

1. **Corrección de Desbordamiento de Pila (Stack Overflow):** Los payloads históricos definían estructuras locales de 10 bytes en la pila para la llamada `ioctl(_IOWR, 28)`, sobrescribiendo 18 bytes de memoria del hilo. Esta suite utiliza estructuras contiguas de 28 bytes alineadas de forma exacta.
2. **Ciclo Seguro Lectura-Modificación-Escritura (RMW):** No se envían comandos ciegos de escritura (`SET`). Se realiza primero una lectura (`GET`), se valida que la tabla térmica corresponda a un perfil de fábrica válido, se altera de forma aislada el byte 5 (umbral) y solo entonces se aplica el cambio.
3. **Mapeo Universal sin Offsets Fijos:** No existen direcciones de memoria de kernel fijas en el código. Las funciones del sistema se resuelven dinámicamente mediante `sys_dynlib_load_prx` y `sys_dynlib_dlsym`.
4. **Limpieza Preventiva en Actualizaciones:** Borrado previo de archivos de actualización incompletos (`PS4UPDATE.PUP`) que hayan podido quedar almacenados en el disco antes de crear los directorios señuelo. (Nota: La discrepancia histórica de modos de permisos en `mkdir` permanece documentada como no resuelta).

---

## 6. Diagnóstico y Validación Comunitaria

La suite incluye la sonda `np_readonly_dual_probe.bin` (y su script complementario `tools/receive_probe.py`), diseñada en modo de solo lectura para permitir a usuarios de cualquier firmware inspeccionar el estado de sus claves de registro de forma completamente segura:

1. Inicie el receptor de telemetría en su PC:
   ```bash
   python tools/receive_probe.py --port 9021
   ```
2. Inyecte la sonda en la consola:
   ```bash
   python tools/send_payload.py --ip <PS4_IP> --port 9090 payloads/v2/np_readonly_dual_probe.bin
   ```
3. Consulte la plantilla en `docs/PROBE_REPORT_TEMPLATE.md` para compartir y contrastar los resultados obtenidos en su modelo y versión de firmware.

---

## 7. Créditos y Licencia

- **GoldHEN:** Desarrollado por SiSTRo y colaboradores.
- **ps4-payload-sdk:** Proyecto original por CTurt y Scene-Collective.
- **OpenOrbis:** Documentación y herramientas para la comunidad homebrew.
- **WPA-Libertario:** Repositorio de referencia histórica sobre rutas del sistema.
- **WebRTE & psdevwiki:** Documentación sobre llamadas del sistema y controladores ICC.

Este proyecto se distribuye bajo los términos de la **Licencia MIT**. Consulte el archivo `LICENSE` para más detalles.

*Descargo de responsabilidad:* Este software se proporciona tal cual, sin garantías de ningún tipo. Su uso es bajo la exclusiva responsabilidad del usuario. Este proyecto no está respaldado por, ni afiliado de ninguna manera con, Sony Interactive Entertainment.

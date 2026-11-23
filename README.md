# 🎮 Payloads Universales PS4: Control de Ventilador & Bloqueo de Updates

[![Release](https://img.shields.io/github/v/release/AJfiles/PS4-Payloads-Updates-Fan?color=blue&label=Versi%C3%B3n)](https://github.com/AJfiles/PS4-Payloads-Updates-Fan/releases/latest)
[![License](https://img.shields.io/badge/Licencia-MIT-green.svg)](LICENSE)
[![Tested on](https://img.shields.io/badge/Hardware-PS4%20Pro%209.00-success.svg)](evidence/TEST_STATUS.md)

Payloads universales para tu PS4: funcionan sin importar la versión del firmware, sin offsets fijos de kernel y sin depender de un archivo distinto por versión. Arreglan de raíz los cuelgues por desbordamiento de memoria en el control del ventilador y te dan un bloqueo en 3 capas reales contra actualizaciones accidentales del sistema.

📦 **¡Descarga directa lista para usar!**
Puedes bajar los paquetes ya compilados directamente desde la [sección de Releases](https://github.com/AJfiles/PS4-Payloads-Updates-Fan/releases/latest):
- 🔐 **`goldhen_full_payloads_v2.zip`**: Suite completa v2 (triple capa de bloqueo + restaurador + sonda + test).
- ⚙️ **`goldhen_universal_payloads.zip`**: Suite v1 clásica (los 8 perfiles de ventilador + bloqueo v1 por carpetas).

---

## 🎮 Catálogo Completo de Payloads

Aquí tienes la lista de todos los binarios incluidos, probados uno a uno con sus sumas SHA-256 para que verifiques su integridad antes de enviar nada a la consola:

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

## 🛡️ Compatibilidad: Dónde Funciona y Cómo Ayudar

Hablamos siempre con la verdad técnica por delante, separando lo medido en nuestro laboratorio de lo que la arquitectura soporta por diseño:

- ✅ **VERIFICADO EN HARDWARE:** PS4 Pro en firmware 9.00 corriendo GoldHEN 2.4b18.10. Aquí probamos y medimos cada payload, comprobando que las 3 capas se aplican en caliente y persisten al reiniciar.
- 🟡 **ESPERADO CON EVIDENCIA:**
  - Ventilador en firmwares **5.05 a 11.00**: funciona mientras exista el nodo `/dev/icc_fan`.
  - Bloqueo v1 en firmwares **4.73 a 13.52**: usa exactamente las mismas rutas de actualización de toda la vida.
- 🔬 **SIN VERIFICAR PERO COMPATIBLE POR DISEÑO (12.00 a 14.02 y FAT/Slim):**
  La arquitectura de estos payloads no depende del número de versión del firmware ni del chasis de tu consola. Solo necesita que se cumplan las cuatro condiciones básicas:
  1. BinLoader activo para recibir el `.bin` crudo.
  2. Privilegios root sin jaula (sandbox escapado).
  3. Nodo `/dev/icc_fan` accesible (para el ventilador).
  4. Llamadas al sistema operativas (`libSceRegMgr` para la suite v2).
  
  **¿Tienes una consola en 12.x, 13.x o 14.02?**
  ¡Te invitamos a probar la sonda de solo lectura! No modifica absolutamente nada en tu consola. Ejecuta `np_readonly_dual_probe.bin` y compártenos tu reporte usando la plantilla [docs/PROBE_REPORT_TEMPLATE.md](docs/PROBE_REPORT_TEMPLATE.md). Un solo reporte desde 14.02 puede confirmar la compatibilidad definitiva para toda la comunidad.

---

## 🧰 Instalación y Cómo Usarlos en el Día a Día

### Lo que necesitas
1. Tu PS4 con GoldHEN cargado.
2. Activar el servidor de payloads:
   - Ve a `Ajustes` > `★GoldHEN` > `Server Settings`.
   - Marca la casilla `Enable BinLoader Server` (puerto por defecto `9090`).

### Cómo se manda un payload 🚀
Desde tu ordenador o terminal en la misma red local:
```bash
python tools/send_payload.py --ip <IP_DE_TU_PS4> --port 9090 payloads/v2/disable_updates_full.bin
```
*(Nota: Si usas el gestor de payloads de GoldHEN en `/data/GoldHEN/payloads/`, ese método figura como alternativo sin verificar por nosotros en este hardware; la inyección directa por red en el puerto 9090 sí está 100% verificada).*

### Flujo diario recomendado
- **Para estar tranquilo:** Inyecta `disable_updates_full.bin` una sola vez. Luego manda tu perfil de ventilador favorito (por ejemplo `fan_threshold_60_universal.bin`). Listo. La protección y el ventilador persisten aunque apagues y vuelvas a encender la consola.
- **Si alguna vez quieres actualizar a propósito:** Manda `enable_updates_full.bin` solo en ese momento. Cuando termines, vuelve a mandar `disable_updates_full.bin`.

---

## ⚠️ Advertencias Importantes

> [!WARNING]
> **`enable_updates_full.bin` deja la consola totalmente expuesta:**
> Al ejecutar este archivo se quitan las carpetas señuelo, se vuelven a encender las descargas automáticas y se reconecta el entorno a `np`. Si te conectas a Internet en ese estado, la consola se bajará e instalará el firmware oficial más reciente, perdiendo el jailbreak para siempre. Úsalo solo si de verdad quieres actualizar.

> [!IMPORTANT]
> **El entorno `noupdt` corta todo acceso a PSN:**
> Esto es deliberado: al poner el entorno en `noupdt`, los servidores de Sony rechazan la conexión. La PlayStation Store, el juego online y la descarga de parches oficiales no funcionarán mientras el bloqueo v2 esté activo. Es el precio de estar 100% protegido.

> [!CAUTION]
> **El umbral de 80 °C no enfría más que el de fábrica:**
> En nuestras mediciones en hardware real, la temperatura de fábrica observada es de **79 °C**. Poner 80 °C le dice al ventilador que arranque más tarde que como venía de fábrica. Lo dejamos solo por mantener todas las opciones históricas de la scene. Recuerda además que el umbral es una meta del ventilador, no un tope máximo que garantice que la consola nunca pase de ahí jugando duro en verano.

---

## 🔧 Qué Arreglan Frente a los Payloads Clásicos

1. **Cero desbordamientos de pila:** Los payloads viejos metían una estructura de 10 bytes en una llamada de 28 bytes, pisando 18 bytes de la pila. Nosotros usamos una estructura contigua exacta de 28 bytes alineada.
2. **Ciclo seguro de lectura antes de escribir (RMW):** Nunca escribimos a ciegas. Primero leemos (`GET`), comprobamos que la tabla térmica sea válida, cambiamos únicamente el byte 5 (la temperatura deseada) y solo entonces escribimos (`SET`).
3. **Código independiente de posición (PIC) real:** Ni un solo offset de kernel hardcodeado. Todo se resuelve en tiempo de ejecución mediante llamadas nativas de carga dinámica.
4. **Limpieza previa de PUPs incompletos:** Si había una actualización a medio descargar, el bloqueador la borra antes de crear las carpetas señuelo.

---

## 🤝 Diagnóstico Comunitario: ¡Súmate!

Queremos que toda la comunidad disfrute de esto con total tranquilidad. Si tienes un firmware distinto a 9.00 (especialmente 12.x, 13.x o **14.02**), ayúdanos a comprobarlo:

1. Abre el receptor en tu PC:
   ```bash
   python tools/receive_probe.py --port 9021
   ```
2. Manda la sonda a tu PS4:
   ```bash
   python tools/send_payload.py --ip <IP_DE_TU_PS4> --port 9090 payloads/v2/np_readonly_dual_probe.bin
   ```
3. Revisa lo que recibe tu PC y compártelo abriendo un Issue o usando la plantilla [docs/PROBE_REPORT_TEMPLATE.md](docs/PROBE_REPORT_TEMPLATE.md). ¡Tu reporte ayuda a todos!

---

## 🙏 Créditos y Agradecimientos

Construido sobre los hombros de gigantes de la scene de PlayStation 4:
- **GoldHEN** por SiSTRo y colaboradores.
- **ps4-payload-sdk** por CTurt y Scene-Collective.
- **OpenOrbis** por sus herramientas y documentación abierta.
- **WPA-Libertario** por la referencia de rutas del sistema.
- **psdevwiki** y **WebRTE** por la documentación de llamadas y controladores.

---

## 📜 Licencia y Descargo de Responsabilidad

Distribuido bajo la **Licencia MIT**. Puedes usarlo, estudiarlo y mejorarlo con total libertad (mira el archivo [LICENSE](LICENSE)).

*Descargo:* Este software se entrega tal cual, sin garantías. Cada cual lo usa bajo su propia responsabilidad. No tenemos ninguna afiliación con Sony Interactive Entertainment.

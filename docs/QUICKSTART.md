# Guía Rápida de Uso (Quickstart v2)

Suite de payloads universales para PS4 / PS4 Pro con GoldHEN.

---

## En la práctica: solo 3 archivos en el uso diario

Para el día a día no necesitas scripts ni diagnósticos; solo envías estos payloads por el **BinLoader (puerto 9090)** según lo que quieras hacer:

```
[ Estado Normal Protegido ]
       │
       ├─► 1. disable_updates_full.bin  (una sola vez: protege disco, sistema y red)
       │
       └─► 2. fan_threshold_60_universal.bin  (o tu umbral preferido 50–75 °C, una sola vez)
```

```
[ Mantenimiento / Juego Online Temporal ]
       │
       ├─► 3. enable_updates_full.bin  (solo cuando quieras jugar online o actualizar)
       │
       └─► ⚠️ REGLA DE ORO: Al terminar, vuelve a enviar disable_updates_full.bin
```

---

## Los Payloads Explicados en 1 Línea

| Binario | Acción | Cuándo usarlo |
|---|---|---|
| `disable_updates_full.bin` | Bloqueo en 3 capas (señuelos en disco + descargas automáticas OFF + NP en `noupdt`) | **Cargar una vez.** Protege la consola de por vida. |
| `fan_threshold_XX_universal.bin` | Fija umbral de ventilador (50, 55, 60, 65, 70, 75 u 80 °C) | **Cargar una vez.** (60 °C es el balance óptimo recomendado). |
| `fan_default_universal.bin` | Restaura el ventilador al valor stock de fábrica (79 °C) | Si quieres el comportamiento acústico de fábrica. |
| `enable_updates_full.bin` | Revierte las 3 capas (retira señuelos + descargas ON + NP en `np`) | **Solo temporalmente** si necesitas conectar a PSN o actualizar. |

> [!WARNING]
> ### Peligro tras ejecutar `enable_updates_full.bin`
> Tras enviar `enable_updates_full.bin`, la consola queda **TOTALMENTE EXPUESTA** a descargar e instalar la última versión del sistema operativo. **Nunca dejes la consola desprotegida.** En cuanto termines la sesión online o de actualización, vuelve a enviar `disable_updates_full.bin`.

---

## Herramientas de Diagnóstico (NO para uso diario)

- **`write_verify_noop.bin`**: **HERRAMIENTA DE DIAGNÓSTICO.** No es para uso diario. Solo se utiliza para validar que las primitivas de escritura de registro (`SetStr` y `SetInt`) funcionan en un firmware nuevo sin alterar ningún ajuste de la consola.
- **`np_readonly_dual_probe.bin`**: Sonda de solo lectura para inspeccionar el estado actual del registro (`NP_env` y `auto_download`) y volcarlo por red TCP al PC.

---

## Manifiesto de Hashes SHA-256 (Paquete v2)

```text
9fcde094b5c0fa6134a9bee0f1823a4506f9549b55778e892f232a101289c6bb  write_verify_noop.bin
d164f3ee4df8e42b67a979f4e5c00101789a842615610273c7046bf9a1ad1043  disable_updates_full.bin
714ed6707080f5a70c00d1c2ca8d9b7e8ad2988ac39dfbac6a9123cacbcd6990  enable_updates_full.bin
a8722089a2e79183480af16bbed9989df88e337a63b0311be34d051f49c90b80  np_readonly_dual_probe.bin
```

---

## Métodos de Carga

1. **Vía Netcat / Script en PC:**
   ```bash
   nc -w 3 <PS4_IP> 9090 < disable_updates_full.bin
   ```
2. **Vía Servidor Web / Teléfono / PC:**
   - Enviar el archivo `.bin` a la IP de la consola en el puerto `9090` con el BinLoader de GoldHEN activo (`Ajustes > ★GoldHEN > Server Settings > Enable BinLoader Server`).

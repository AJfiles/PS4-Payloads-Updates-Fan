# Instalación y carga

## Requisitos

- PS4 con GoldHEN cargado.
- BinLoader Server habilitado: **Settings → ★ GoldHEN → Server Settings → Enable BinLoader Server**.
- Para estos payloads, el proceso debe tener UID 0 y `is_in_sandbox() == 0`. El bin aborta y notifica si no cumple la guarda.
- Los payloads fan requieren `/dev/icc_fan` de tipo dispositivo de caracteres y acceso suficiente. Si no existe o GET falla, el fan aborta sin SET.
- PC y PS4 en la misma LAN. BinLoader GoldHEN: TCP 9090. El log payload→PC usa la dirección `<PS4_IP>:9090` compilada actualmente; recompila si cambia la IPv4 del PC.

## Comprobación y envío

Desde la raíz del proyecto, verifica el hash del binario elegido contra `payloads/build/SHA256SUMS.txt` y cárgalo:

```powershell
certutil -hashfile payloads/build/fan_threshold_60_universal.bin SHA256
python tools/send_payload.py --ps4-ip <PS4_IP> --port 9090 --bin payloads/build/fan_threshold_60_universal.bin
```

El sender confirma que transmitió los bytes; el reporte recibido en el listener TCP confirma que el payload llegó a enviar su log. Para una operación con sonda usa el runner integrado según `docs/TESTING.md`. No abras una conexión de prueba vacía a 9090: puede consumir el accept del BinLoader.

## Efectos persistentes y precauciones

- Fan se aplica por ioctl ICC; en PS4 Pro 9.00 con GoldHEN 2.4b18.10 la sonda C leyó `0x3C` dos veces después del reinicio reportado. El objetivo ICC no es una cota de temperatura: un juego demandante puede superar 75 °C sin que eso determine si el byte persistió. Aplica default 79 para volver al valor stock observado; ese preset no restaura una tabla previa.
- Disable updates altera las entradas filesystem bajo `/update` y persiste al reinicio. Al encontrar un fichero regular en cualquiera de las rutas, lo elimina y el contenido/PUP se pierde. El payload informa la ruta y el tamaño antes del unlink.
- Enable updates solo retira directorios vacíos en las dos rutas canónicas; el modo y propietario no importan. Si encuentra contenido o no puede confirmar que están vacíos, aborta. Nunca uses borrado recursivo; el binario tampoco lo hace.
- Para F, desactiva en la consola **Ajustes → Sistema → Descargas automáticas → Archivos de actualización del software del sistema** antes de ejecutar F0/F. Mantén la consola despierta y no aceptes avisos. La conectividad de red permanece activa; el riesgo de que ocurra otra acción del sistema durante enable es bajo, pero no cero.

## Cobertura

VERIFICADO: PS4 Pro 9.00 + GoldHEN 2.4b18.10; contexto root/sandbox, nodo ICC, GET, todos los umbrales y lectura C `0x3C` tras el reinicio reportado. C es una lectura del estado de ajuste, no demuestra control de la temperatura máxima. Los smoke tests están en [FAN_SMOKE_TESTS.md](../analysis/FAN_SMOKE_TESTS.md). El roundtrip histórico F está en [probe_F.txt](../analysis/probe_F.txt); la revalidación de los BIN de updates reconstruidos tiene un reporte separado. Para abrir una ventana sin los señuelos exige descargas automáticas desactivadas y `NP environment = noupdt`; no aceptes avisos durante la medida. La versión mínima GoldHEN no está establecida; otras combinaciones son experimentales o desconocidas.

# Consideraciones de Seguridad y Mecanismos de Protección

## 1. Advertencias Importantes
- **enable_updates_full.bin:** Elimina las restricciones de actualización, reactiva las descargas automáticas y restaura el entorno de red oficial. Conectar la consola a Internet en este estado descargará el firmware más reciente, anulando el acceso a homebrew.
- **Entorno NP noupdt:** Bloquea por completo el acceso a los servidores oficiales de red (PSN, tienda y parches en línea).
- **Umbral térmico de 80 °C:** El valor de fábrica del hardware es 79 °C. Configurar 80 °C establece un objetivo térmico superior al de fábrica y no produce refrigeración adicional.
- **Objetivo vs. Techo:** El umbral de temperatura es un valor meta para el controlador del ventilador; no garantiza un límite físico máximo bajo carga térmica extrema.

## 2. Mecanismos a Prueba de Fallos
- **Lectura-Modificación-Escritura (RMW):** El control de ventilador lee la tabla completa de 28 bytes mediante GET, modifica únicamente el byte 5 y solo ejecuta SET si la estructura es válida.
- **Borrado de archivos residuales:** Al activar el bloqueo, si se detecta un archivo real de actualización descargado previamente, este se elimina antes de crear el directorio señuelo.
- **Degradación elegante:** Si alguna capa del registro no puede modificarse, se mantiene activa la protección en el sistema de archivos.

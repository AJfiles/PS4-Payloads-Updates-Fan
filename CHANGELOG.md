# Registro de Cambios (Changelog)

## [2.0.0] - 2026-11-23
### Añadido
- Suite de actualización v2 con protección de triple capa (sistema de archivos, desactivación de descargas automáticas y entorno NP de sistema en noupdt).
- Binario write_verify_noop.bin para comprobación segura de primitivas SetStr y SetInt en hardware sin alteración de estado.
- Sonda de telemetría de doble clave np_readonly_dual_probe.bin con reporte estructurado por socket TCP.
- Reversión completa de tres capas en enable_updates_full.bin.

## [1.0.0] - 2026-11-20
### Añadido
- Suite de control térmico universal (50 °C a 80 °C en pasos de 5 °C y valor por defecto de 79 °C) con operación Lectura-Modificación-Escritura de 28 bytes y sin desbordamiento de pila.
- Suite de actualización v1 basada en directorios señuelo con permisos completos.
- Runtime independiente de posición (PIC) con resolución dinámica en tiempo de ejecución de librerías del sistema sin offsets hardcodeados de kernel.

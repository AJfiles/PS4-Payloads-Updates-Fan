# Matriz de Compatibilidad y Condiciones de Ejecución

## 1. Niveles de Soporte Técnico
- **VERIFICADO EN HARDWARE REAL:** PS4 Pro (CUH-7015B) en firmware 9.00 con GoldHEN 2.4b18.10. Todos los binarios probados con verificación funcional y de persistencia.
- **ESPERADO CON ALTA CERTEZA:**
  - Control de ventilador (5.05 a 11.00): Si el nodo `/dev/icc_fan` está presente y operativo.
  - Actualizaciones v1 (4.73 a 13.52): Mecanismo de señuelos estándar verificado contra referencias históricas.
- **DESCONOCIDO:** Firmwares 12.00 a 14.00, modelos Fat y Slim (misma ABI de kernel pero sin pruebas de laboratorio en esta serie).
- **NO COMPATIBLE:** Firmwares superiores a 14.00. No se debe prometer compatibilidad en versiones no documentadas ni inventar porcentajes.

## 2. Precondiciones Requeridas en Tiempo de Ejecución
1. Servidor de carga de binarios crudos (BinLoader en puerto 9090).
2. Proceso con privilegios de superusuario (UID 0, GID 0) y sin jaula de seguridad (sandbox escapado).
3. Dispositivo de caracteres `/dev/icc_fan` accesible en modo lectura/escritura (para fan control).
4. Llamadas al sistema y resolución dinámica funcional (`sys_dynlib_load_prx` para libSceRegMgr en v2).

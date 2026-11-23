# NP environment en PS4: evidencia y límites

## Trabajo aparcado para otra sesión

NP Environment no se implementa ni se modifica desde este proyecto. El ajuste solo se confirmó manualmente como `noupdt` para las ventanas de prueba; el payload no lo leyó y no hay código que lo escriba. Trátese como proyecto separado y conserve la lectura/guardar/restaurar exacto como requisito antes de cualquier implementación.

Fecha de consulta: 2026-11-19. Esta nota es investigación opcional posterior a F; no forma parte de `dist/goldhen_universal_payloads.zip` y no modifica la consola.

## Hallazgos

| Pregunta | Hallazgo | Calidad / límite |
|---|---|---|
| ¿Existe el ajuste? | PSDevWiki enumera `NP Environment` bajo Debug Settings → PlayStation Network como ajuste del entorno de PSN. | Documenta el ajuste, pero no identifica su clave de registro. [PSDevWiki: Debug Settings](https://www.psdevwiki.com/ps4/%E2%98%85_Debug_Settings) |
| ¿Hay clave candidata? | `np-account-restore` define `REG_KEY_NP_ENV = 125874183` (`0x0780B007`) y documenta que copia una cadena de 17 bytes desde `config.dat` offset `0x177`; al leer/escribir una cuenta suma el slot `(n-1)*65536`. | Evidencia de código de homebrew comunitario en commit `e6477fb4a1e585f7b8dbf7efc32636797f9d9e3b`. No demuestra por sí sola que el menú Debug Settings use exactamente esa entrada ni que todos los firmwares/modelos compartan el mismo comportamiento. [Fuente fijada](https://github.com/earthonion/np-account-restore/blob/e6477fb4a1e585f7b8dbf7efc32636797f9d9e3b/np-restore-account.c) |
| ¿Puede leerse/escribirse por API? | La lista PS4 de `libSceRegMgr` incluye `sceRegMgrGetStr` y `sceRegMgrSetStr`. El payload citado declara ambas APIs y llama a `GetStr`/`SetStr` para campos de cuenta. | Prueba de que existen exports y de que un proyecto de payload los invoca; ese código no comprueba los códigos de retorno de todas las escrituras. No acredita todavía acceso funcional a esta clave en la PS4 Pro 9.00 concreta. [PSDevWiki: libSceRegMgr](https://www.psdevwiki.com/ps4/LibSceRegMgr), [fuente fijada](https://github.com/earthonion/np-account-restore/blob/e6477fb4a1e585f7b8dbf7efc32636797f9d9e3b/np-restore-account.c) |
| ¿Qué escribe el menú? | No encontrado en fuentes consultadas. La igualdad entre `REG_KEY_NP_ENV` y el ajuste del menú es una hipótesis razonable por nombre y contenido, no un mapeo confirmado del código de Debug Settings. | Sin evidencia directa de código/símbolos del módulo de ajustes o lectura comparativa antes/después en hardware. |
| ¿Persiste y qué afecta? | Una guía de la escena afirma que cambiarlo persiste tras reinicio/suspensión/red y que valores distintos de `np` impiden servicios de PSN; enumera Store, Remote Play y ciertos flujos de actualizaciones como afectados. | Fuente secundaria; no se generaliza a todos los valores, apps o firmwares. La prueba F solo registra que el operador confirmó `noupdt`; no leyó ese valor desde el registro. [ConsoleMods: Blocking OFW Updates](https://consolemods.org/wiki/PS4%3ABlocking_OFW_Updates) |
| ¿Cómo restaurar exactamente? | Primero habría que leer y guardar la cadena actual para el slot de usuario correcto con `sceRegMgrGetStr`; para revertir, escribir exactamente ese valor guardado con `sceRegMgrSetStr`. | No asumir que el original es `np`, ni usar solo un texto predeterminado. El slot y la clave exacta deben confirmarse antes. Para soportar reinicio, el snapshot tendría que sobrevivir en USB u otro medio elegido; todavía no hay diseño/implementación. |

## Decisión

**No implementar ni proponer todavía un par de payloads listo para usar.** Hay evidencia suficiente para identificar `0x0780B007` como clave candidata de cadena NP Environment por cuenta y para señalar las APIs de lectura/escritura, pero no para afirmar que Debug Settings modifica esa clave en este contexto ni para elegir el slot activo con seguridad. La fase queda **BLOQUEADA para implementación**, no para investigación.

Siguiente prueba segura si se retoma esta opción: añadir una consulta estrictamente de solo lectura de `sceRegMgrGetStr` para los slots candidatos, registrar clave, slot, retorno y cadena antes y después de que el usuario cambie NP Environment manualmente. No llamar `SetStr`. La comparación determinaría el mapeo; luego un diseño de activación/restauración tendría que guardar el valor original y verificar cada retorno. No se incluyó esa consulta en la sonda de ICC ya ejecutada.

Si posteriormente se implementara, el cambio de entorno afecta conectividad y servicios asociados a PSN. Debe tratarse como una capa independiente del bloqueo de PUP, exigir una copia recuperable del valor anterior y abortar si no puede leer/guardar/confirmar el estado exacto.

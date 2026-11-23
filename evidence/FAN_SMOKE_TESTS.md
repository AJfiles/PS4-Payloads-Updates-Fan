# Pruebas de umbrales de ventilador

Hardware: PS4 Pro, firmware 9.00, GoldHEN 2.4b18.10 (versión confirmada por el usuario). Cada prueba envió un binario permitido y después la sonda. El runner verificó el SHA-256 antes de cada envío y validó que el GET posterior coincidiera con la tabla escrita.

| Medida | Byte 5 antes → después | Diferencias en tabla | Resultado |
|---|---:|---:|---|
| T50 | `3C → 32` | solo offset `0x05` (1/28) | VERIFICADO; valor bajo aceptado |
| T55 | `32 → 37` | solo offset `0x05` (1/28) | VERIFICADO |
| T60 (E2) | `4F → 3C` | solo offset `0x05` (1/28) | VERIFICADO; aviso visible y aumento audible confirmados por el usuario |
| T65 | `37 → 41` | solo offset `0x05` (1/28) | VERIFICADO |
| T70 | `41 → 46` | solo offset `0x05` (1/28) | VERIFICADO |
| T75 | `46 → 4B` | solo offset `0x05` (1/28) | VERIFICADO |
| T80 | `4B → 50` | solo offset `0x05` (1/28); aviso `80_exceeds_stock_79` presente | VERIFICADO con aviso de valor superior al stock observado |
| Default 79 | `50 → 4F` | solo offset `0x05` (1/28) | VERIFICADO; aplica 79, no restaura snapshot |

La captura final del smoke test terminó en `0x4F` (79). Después, F0 leyó nuevamente `0x4F`; entre TDEFAULT y F0 no se envió ningún fan payload. La temperatura observada posteriormente en un juego demandante superó 75 °C; esto no cambia la evidencia de byte ICC y no es una prueba controlada de eficacia térmica. El umbral es un objetivo de ventilador, no una garantía de temperatura máxima. Cada informe guarda el log completo y el diff:

- `probe_T50.txt`, `probe_T55.txt`, `probe_T65.txt`, `probe_T70.txt`, `probe_T75.txt`, `probe_T80.txt`, `probe_TDEFAULT.txt`
- `probe_E2.txt` registra el fan60 universal corregido.

Estos resultados verifican los payloads en esta combinación concreta de consola y GoldHEN; no se extrapolan a otros firmwares o modelos. F0 preservó las rutas root-owned vacías `0555`; después, F completó enable → disable y la sonda confirmó ambas rutas vacías `0777`. Para F, el operador confirmó `NP environment = noupdt` mediante el flag del runner; no hubo lectura independiente de ese ajuste. Ver [UPDATES_TESTS.md](UPDATES_TESTS.md) y [C_AND_F_AUDIT.md](C_AND_F_AUDIT.md).

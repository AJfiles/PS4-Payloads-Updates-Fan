# PS4 Universal Payloads: Fan Control & Updates Blocker

A robust, position-independent (PIC) suite of PlayStation 4 payloads engineered to execute universally across firmware versions without hardcoded kernel offsets. It fixes legacy stack overflow bugs in fan control and provides an empirical three-layer protection against accidental system updates.

---

## Payload Summary

| Payload | Description | Immediate Effect | Reboot Persistence | Risk Level | SHA-256 Checksum |
|---|---|---|---|---|---|
| `fan_threshold_50_universal.bin` | Fan Control 50 °C | RPM raises at 50 °C | Persists across reboots | Low (higher noise) | `c09533b4811fd49642a1d6bdea84973da0227aa80c5c3f38f8fdbcaa76fbf549` |
| `fan_threshold_55_universal.bin` | Fan Control 55 °C | RPM raises at 55 °C | Persists across reboots | Low | `e8763eaae27248138ea1d37234919acaba93db1a13b0b1ee1f089695ae69a54f` |
| `fan_threshold_60_universal.bin` | Fan Control 60 °C | RPM raises at 60 °C | Persists across reboots | Low (recommended sweetspot) | `7ee35577a903899037630ad5d201dc9a03aa480dbf9b757753186375ce702ad0` |
| `fan_threshold_65_universal.bin` | Fan Control 65 °C | RPM raises at 65 °C | Persists across reboots | Low | `6e3d3f0f87e687a2f8f3ab202da3446fdd3a32e40a4784027e75dbd91ec2197a` |
| `fan_threshold_70_universal.bin` | Fan Control 70 °C | RPM raises at 70 °C | Persists across reboots | Low | `31f8ab3898134dda855e2737c656b387e7738bf8a997f6bc5939c5590827b017` |
| `fan_threshold_75_universal.bin` | Fan Control 75 °C | RPM raises at 75 °C | Persists across reboots | Low | `76676ff8d86382c95cab5d6ef4d07df25e5fdb79112566246c845c42c6f07df8` |
| `fan_threshold_80_universal.bin` | Fan Control 80 °C | RPM raises at 80 °C | Persists across reboots | Moderate (above 79 °C stock) | `bf31765ed3de71955c5a77aa101cf1118dabd0bb587f3d43c11c35cb397a3e94` |
| `fan_default_universal.bin` | Restore Stock Thermal | Restores stock 79 °C | Persists across reboots | None (original factory profile) | `d2d26d032bccbc0aa9499ad9ab71549c7569a584264074f4f804c13d148f46b2` |
| `disable_updates_universal.bin` | v1 Decoy Blocker | Creates immutable decoy dirs | Persists across reboots | Low (PSN stays reachable) | `0a3ae44cf6a691c17432b734687a5d386bc5561298985e882a92b5f6d381db62` |
| `enable_updates_universal.bin` | v1 Decoy Remover | Removes decoy directories | Persists across reboots | High if connected to network | `c7cb5370041a982a54cb1cb2cc1c004d435451f5d0b376d93c372cee47e4541a` |
| `disable_updates_full.bin` | v2 3-Layer Blocker | Decoys + auto_download 0 + noupdt | Persists across reboots | Low (blocks PSN connection) | `d164f3ee4df8e42b67a979f4e5c00101789a842615610273c7046bf9a1ad1043` |
| `enable_updates_full.bin` | v2 3-Layer Restorer | Cleans decoys + auto_download 1 + np | Persists across reboots | Critical: exposed to updates | `714ed6707080f5a70c00d1c2ca8d9b7e8ad2988ac39dfbac6a9123cacbcd6990` |
| `write_verify_noop.bin` | Safe Write Test | Writes current values & verifies | No state modification | None (verification test) | `9fcde094b5c0fa6134a9bee0f1823a4506f9549b55778e892f232a101289c6bb` |
| `np_readonly_dual_probe.bin` | Diagnostic Probe | Inspects keys & reports via TCP | Pure read-only operation | None (non-intrusive probe) | `a8722089a2e79183480af16bbed9989df88e337a63b0311be34d051f49c90b80` |

---

## Key Highlights

1. **Bug-Free Memory Safety:** Eliminates the classic 18-byte stack overflow present in legacy fan payloads by using an exact 28-byte aligned structure and read-modify-write safety checks.
2. **True Position-Independent Code (PIC):** Dynamic symbol resolution via runtime system calls (`sys_dynlib_load_prx` and `sys_dynlib_dlsym`). No hardcoded kernel addresses.
3. **Triple-Layer Update Blocking:**
   - Layer 1: Filesystem decoy directories blocking update packages.
   - Layer 2: Disables background OS automatic downloads via registry key `0x02800500`.
   - Layer 3: Environment isolation via system NP environment key `0x19800000` set to `noupdt`.
4. **Verified Hardware Execution:** Tested and measured on PS4 Pro running GoldHEN 2.4b18.10.

---

## Quickstart

1. Ensure GoldHEN is running on your console.
2. Navigate to `Settings` > `★GoldHEN` > `Server Settings` and enable `Enable BinLoader Server` (port 9090).
3. Send the payload from your terminal:
   ```bash
   python tools/send_payload.py --ip <PS4_IP> --port 9090 payloads/v2/disable_updates_full.bin
   ```

Distributed under the **MIT License**. See `LICENSE` for details. Not affiliated with Sony Interactive Entertainment.

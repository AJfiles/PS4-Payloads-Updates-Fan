# 🎮 PS4 Universal Payloads: Fan Control & Updates Blocker

[![Release](https://img.shields.io/github/v/release/AJfiles/PS4-Payloads-Updates-Fan?color=blue&label=Release)](https://github.com/AJfiles/PS4-Payloads-Updates-Fan/releases/latest)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Tested on](https://img.shields.io/badge/Hardware-PS4%20Pro%209.00-success.svg)](evidence/TEST_STATUS.md)

🧪 **Hardware tested:** PS4 Pro, firmware 9.00, GoldHEN 2.4b18.10
🎯 **Expected range:** 5.05–11.00 (fan) | 4.73–13.52 (updates v1) | 12.00–14.02 (to be verified)

Universal, position-independent (PIC) payloads for your PlayStation 4: they work seamlessly across firmware revisions with no hardcoded kernel addresses and no need for separate files per version. They fix legacy memory corruption crashes in fan control and deliver genuine 3-layer protection against accidental system updates.

📦 **Ready-to-use compiled bundles:**
Download prebuilt archives directly from our [Releases page](https://github.com/AJfiles/PS4-Payloads-Updates-Fan/releases/latest):
- 🔐 **`goldhen_full_payloads_v2.zip`**: Complete v2 suite (3-layer update blocker + restorer + diagnostic probe + safe write test).
- ⚙️ **`goldhen_universal_payloads.zip`**: Classic v1 suite (8 fan control profiles + v1 filesystem decoy blocker).

---

## 🎮 Payload Catalog

All 14 binaries compiled and measured with authoritative SHA-256 checksums:

| Payload | Description | Immediate Effect | Reboot Persistence | Risk Level | SHA-256 Checksum |
|---|---|---|---|---|---|
| `fan_threshold_50_universal.bin` | Fan Control 50 °C | RPM raises at 50 °C | Persists across reboots | Low (higher noise) | `cf4ab65eab24c7ceb6a25b44dc5c3372fdad565ce52237e6c503004b57c14370` |
| `fan_threshold_55_universal.bin` | Fan Control 55 °C | RPM raises at 55 °C | Persists across reboots | Low | `724d242c0bc273234b664a18e6a316fb705ffa002d6b4c2a85ad010401577628` |
| `fan_threshold_60_universal.bin` | Fan Control 60 °C | RPM raises at 60 °C | Persists across reboots | Low (recommended sweetspot) | `27f128f57929098734b92ac636e887dd7d54c67b603f3c70a9f0fede4baf5f8f` |
| `fan_threshold_65_universal.bin` | Fan Control 65 °C | RPM raises at 65 °C | Persists across reboots | Low | `aac2123cb814c57dddc6b50cedfab7d7d710ce94c277df0ec79b88c0aedcbe42` |
| `fan_threshold_70_universal.bin` | Fan Control 70 °C | RPM raises at 70 °C | Persists across reboots | Low | `17c73f71f9a233a50c10091c3b0f114e6805d3326496b4ef4fffe8843184829a` |
| `fan_threshold_75_universal.bin` | Fan Control 75 °C | RPM raises at 75 °C | Persists across reboots | Low | `0f60b6ee0bfd8809474599d7e1ac82bcbad81f9c524b1a5d77633c26dc5de14d` |
| `fan_threshold_80_universal.bin` | Fan Control 80 °C | RPM raises at 80 °C | Persists across reboots | Moderate (above 79 °C stock) | `705caa0e56e1bb287051cdeabea60cea489aa53cfcc0a700e4ec29cc3c7266de` |
| `fan_default_universal.bin` | Restore Stock Thermal | Restores stock 79 °C | Persists across reboots | None (original factory profile) | `de339b0a3d62306cc746eb40d1ee19d465e64012ce8f6cd67a3799922b7c94e0` |
| `disable_updates_universal.bin` | v1 Decoy Blocker | Creates immutable decoy dirs | Persists across reboots | Low (PSN stays reachable) | `db051b134a1909f225064bd92c2de3cb4f67b24d4c93f2192c3a067bbf77b911` |
| `enable_updates_universal.bin` | v1 Decoy Remover | Removes decoy directories | Persists across reboots | High if connected to network | `d13f187d387efb4be61fdd0937792810b4774f127346eb0e453aca63680770a3` |
| `disable_updates_full.bin` | v2 3-Layer Blocker | Decoys + auto_download 0 + noupdt | Persists across reboots | Low (blocks PSN connection) | `669e2b988c27ee7857c1234a276a25be9164a3f7cb9735f4f6a618157a809a25` |
| `enable_updates_full.bin` | v2 3-Layer Restorer | Cleans decoys + auto_download 1 + np | Persists across reboots | Critical: exposed to updates | `1ab92bf5be27a11a792e5ce5f0adb94a4ba2da3b0cf1949a1722a6fc4715a314` |
| `write_verify_noop.bin` | Safe Write Test | Writes current values & verifies | No state modification | None (verification test) | `d43f7f60f3430e64b6b011d432a1ad634e78aac8cedaaa8d5ce6d405ab72888d` |
| `np_readonly_dual_probe.bin` | Diagnostic Probe | Inspects keys & reports via TCP | Pure read-only operation | None (non-intrusive probe) | `d4dc8c3b1ed4c9bc3ffd2d236de4213118f44bf9e29b9bb61d1437440df10d53` |

---

## 🛡️ Compatibility: Tested Reality & Community Invitation

- ✅ **HARDWARE VERIFIED:** PS4 Pro on firmware 9.00 running GoldHEN 2.4b18.10. Every payload was benchmarked and verified for immediate effect and reboot persistence.
- 🟡 **EXPECTED WITH STRONG EVIDENCE:**
  - Fan control across **5.05 to 11.00**: Operational as long as `/dev/icc_fan` is present.
  - Updates v1 across **4.73 to 13.52**: Uses identical standard filesystem update decoy paths.
- 🔬 **UNVERIFIED BUT COMPATIBLE BY DESIGN (12.00 to 14.02 & FAT/Slim):**
  These payloads are fully agnostic of the specific firmware integer or chassis revision. They only require:
  1. BinLoader server active on port 9090.
  2. Root privileges without sandbox.
  3. `/dev/icc_fan` character device (for fan control).
  4. Working syscall resolution (`libSceRegMgr` for v2).

  **Running 12.x, 13.x, or 14.02?**
  You can run our completely safe, non-intrusive probe `np_readonly_dual_probe.bin` and submit your telemetry output using [docs/PROBE_REPORT_TEMPLATE.md](docs/PROBE_REPORT_TEMPLATE.md). A single report from 14.02 can validate full compatibility for everyone!

---

## 🧰 Quickstart

1. Ensure GoldHEN is running on your console.
2. Enable BinLoader Server under `Settings` > `★GoldHEN` > `Server Settings` (default port `9090`).
3. Send the payload from your terminal:
   ```bash
   python tools/send_payload.py --ip <YOUR_PS4_IP> --port 9090 payloads/v2/disable_updates_full.bin
   ```

---

## 👤 Author

**AJ** — Development, testing, and documentation of this universal payload suite.
- Repository: [github.com/AJfiles/PS4-Payloads-Updates-Fan](https://github.com/AJfiles/PS4-Payloads-Updates-Fan)
- Tested on real hardware (PS4 Pro 9.00)
- Embedded signature in all binaries for traceability

---

Distributed under the **MIT License**. See `LICENSE` for details. Not affiliated with Sony Interactive Entertainment.

# 🎮 PS4 Universal Payloads: Fan Control & Updates Blocker

[![Release](https://img.shields.io/github/v/release/AJfiles/PS4-Payloads-Updates-Fan?color=blue&label=Release)](https://github.com/AJfiles/PS4-Payloads-Updates-Fan/releases/latest)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Tested on](https://img.shields.io/badge/Hardware-PS4%20Pro%209.00-success.svg)](evidence/TEST_STATUS.md)

🧪 **Hardware tested:** PS4 Pro, firmware 9.00, GoldHEN 2.4b18.10
🎯 **Expected range:** 5.05–11.00 (fan) | 4.73–13.52 (updates v1) | 12.00–14.02 (to be verified)

Universal, position-independent (PIC) payloads for your PlayStation 4: they work seamlessly across firmware revisions with no hardcoded kernel addresses and no need for separate files per version. They fix legacy memory corruption crashes in fan control and deliver genuine 3-layer protection against accidental system updates.

---

## 📦 Downloads

### goldhen_full_payloads_v2.1.1.zip (RECOMMENDED)
**Complete 3-layer update blocker + all fan temperature thresholds**

Includes:
- 2 updates v2 payloads (`disable_updates_full.bin`, `enable_updates_full.bin`)
- 8 fan control payloads (50–80 °C + default 79 °C)
- `diagnostic/` folder containing validation tools (`write_verify_noop.bin`, `np_readonly_dual_probe.bin`)

**Daily usage:**
1. `disable_updates_full.bin` (once, persists across reboots)
2. `fan_threshold_60_universal.bin` (once, persists across reboots)

**To update or access PSN:**
1. `enable_updates_full.bin`
2. Update / use PSN
3. `disable_updates_full.bin` immediately afterwards

⚠️ **Warning:** The v2 blocker cuts off PSN access (Store, multiplayer, game patches).

### goldhen_universal_payloads_v2.1.1.zip (ALTERNATIVE)
**Minimal filesystem decoy blocker + all fan temperature thresholds**

Includes:
- 2 updates v1 payloads (`disable_updates_universal.bin`, `enable_updates_universal.bin`)
- 8 fan control payloads (50–80 °C + default 79 °C)

**Differences from v2:**
- Only blocks system PUP downloads via decoy folders (does not touch auto-download or NP_env registry keys)
- PSN remains reachable (though system update prompt may appear)
- More portable, less comprehensive protection

---

## 🎮 Payload Catalog

All 14 binaries compiled and measured with authoritative SHA-256 checksums:

| Payload | Description | Immediate Effect | Reboot Persistence | Risk Level | SHA-256 Checksum |
|---|---|---|---|---|---|
| `fan_threshold_50_universal.bin` | Fan Control 50 °C | RPM raises at 50 °C | Persists across reboots | Low (higher noise) | `5df818ab7cc4529a233a31397338e7657952cf94fe2863f8de508aeda71a4540` |
| `fan_threshold_55_universal.bin` | Fan Control 55 °C | RPM raises at 55 °C | Persists across reboots | Low | `fd5eb4e897289bf7c2ae76a10de793bbed542aa3d93364940111850b6e23620a` |
| `fan_threshold_60_universal.bin` | Fan Control 60 °C | RPM raises at 60 °C | Persists across reboots | Low (recommended sweetspot) | `72bfc7ae8b93e780f574d3d5f552110e36955eb178b5a50e9e2f4c38fc7146f5` |
| `fan_threshold_65_universal.bin` | Fan Control 65 °C | RPM raises at 65 °C | Persists across reboots | Low | `c57b262d0a3254787a2acf674a12b8e64f3a05053d1b82013f4573eabf84ed6e` |
| `fan_threshold_70_universal.bin` | Fan Control 70 °C | RPM raises at 70 °C | Persists across reboots | Low | `afcab9ef28942cf9a0af05dc2965074a9d06b1f8ab555b36b53937088b49cd94` |
| `fan_threshold_75_universal.bin` | Fan Control 75 °C | RPM raises at 75 °C | Persists across reboots | Low | `a0e23146c8680a0728b21af7b439ee063399bef3dcbcbe24d9cdcc9b2528c7b9` |
| `fan_threshold_80_universal.bin` | Fan Control 80 °C | RPM raises at 80 °C | Persists across reboots | Moderate (above 79 °C stock) | `b3ece1b5d05178ea1de87e61afa01c1b3716921fad3ca5aefc4e0e2a024e913d` |
| `fan_default_universal.bin` | Restore Stock Thermal | Restores stock 79 °C | Persists across reboots | None (original factory profile) | `1660b849ec15fc529f22382a3ef98facb5c8762b74be6558ff13b3296e01ea1d` |
| `disable_updates_universal.bin` | v1 Decoy Blocker | Creates immutable decoy dirs | Persists across reboots | Low (PSN stays reachable) | `6b27067b10a0455f89f1cc9ace6f40b810b29e95ce49dffda69e3c34f14177d1` |
| `enable_updates_universal.bin` | v1 Decoy Remover | Removes decoy directories | Persists across reboots | High if connected to network | `a5ce2a0fc9aafe499edf214b377b6dbeb6a89e23bd5be7fd02e8bac499132afb` |
| `disable_updates_full.bin` | v2 3-Layer Blocker | Decoys + auto_download 0 + noupdt | Persists across reboots | Low (blocks PSN connection) | `93d2285e33840113d268994bc9c8c63da25b758ea73f847d51c0153cf91c1e6d` |
| `enable_updates_full.bin` | v2 3-Layer Restorer | Cleans decoys + auto_download 1 + np | Persists across reboots | Critical: exposed to updates | `530f60e00357bcaafdbc3f4d3be87e645b85bd3a41bdd01cc35a4c8515a5efe0` |
| `write_verify_noop.bin` | Safe Write Test | Writes current values & verifies | No state modification | None (verification test) | `4b125b6c1229c7dfa8c3d8e8961e0f89d8c6faabe1296b606af6d6ad4a218f8d` |
| `np_readonly_dual_probe.bin` | Diagnostic Probe | Inspects keys & reports via TCP | Pure read-only operation | None (non-intrusive probe) | `a32dcc1a53cc95ee2a4b846334dc0af4b854fd4dd450f11bbcc2e8796b1b2983` |

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

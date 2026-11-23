#!/usr/bin/env python3
"""Audita las syscalls y constantes incluidas en cada BIN raw universal."""
from __future__ import annotations

import argparse
import hashlib
import ipaddress
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_64

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "payloads" / "build"
COMMON = {585, 591, 594}
FAN_ALLOWED = COMMON | {5, 6, 54, 190}
DISABLE_ALLOWED = COMMON | {5, 6, 10, 60, 136, 190, 272}
ENABLE_ALLOWED = COMMON | {5, 6, 60, 136, 137, 190, 272}
FAN_NAMES = [f"fan_threshold_{v}_universal.bin" for v in (50, 55, 60, 65, 70, 75, 80)] + ["fan_default_universal.bin"]
ALL_NAMES = FAN_NAMES + ["disable_updates_universal.bin", "enable_updates_universal.bin"]
NOTICE_COMMON = [
    b"sceKernelSendNotificationRequest",
    b"Carga GoldHEN antes de este payload",
]
NOTICE_FAN = [
    b"Ventilador ajustado: ",
    b"Ventilador: 80 ",
    b"Superior al original",
    b"umbral original restaurado (79 ",
    b"Este GoldHEN no expone el control del ventilador",
    b"No se pudo leer el ventilador. Sin cambios",
    b"Error en ventilador. Estado requiere revision",
    bytes.fromhex("c2 b0"),  # grado codificado en UTF-8
]
NOTICE_UPDATES = [
    b"Actualizaciones bloqueadas. Se mantienen al apagar",
    b"Actualizaciones habilitadas. Pueden descargarse",
]


def find_syscalls(raw: bytes) -> tuple[set[int], int]:
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    ins = list(md.disasm(raw, 0))
    dispatcher = None
    for i in range(len(ins) - 1):
        if ins[i].mnemonic == "mov" and ins[i].op_str.replace(" ", "") == "r10,rcx" and ins[i + 1].mnemonic == "syscall":
            dispatcher = ins[i].address
            break
    if dispatcher is None:
        raise ValueError("falta dispatcher syscall del SDK")
    nums: set[int] = set()
    for i in range(len(ins) - 1):
        if ins[i].mnemonic == "mov" and ins[i].op_str.startswith("rax, ") and ins[i + 1].mnemonic == "jmp":
            try:
                num = int(ins[i].op_str.split(",", 1)[1].strip(), 0)
                target = int(ins[i + 1].op_str, 0)
            except ValueError:
                continue
            if target == dispatcher:
                nums.add(num)
    return nums, sum(i.mnemonic == "syscall" for i in ins)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report-host", required=True)
    parser.add_argument("--ps4-ip", default="<PS4_IP>")
    args = parser.parse_args()
    try:
        host = str(ipaddress.IPv4Address(args.report_host)).encode("ascii")
        ps4 = str(ipaddress.IPv4Address(args.ps4_ip)).encode("ascii")
    except ipaddress.AddressValueError as exc:
        parser.error(f"IPv4 inválida: {exc}")
    if host == ps4:
        parser.error("el receptor y la PS4 no pueden compartir la misma IP")

    manifest = (OUT / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines()
    expected_hashes = {line.split()[1]: line.split()[0] for line in manifest if line.strip()}
    errors: list[str] = []
    for name in ALL_NAMES:
        path = OUT / name
        if not path.is_file():
            errors.append(f"{name}: falta BIN")
            continue
        raw = path.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if expected_hashes.get(name) != digest:
            errors.append(f"{name}: hash no coincide con SHA256SUMS")
        if not raw or raw[0] != 0xE9:
            errors.append(f"{name}: entrada no es salto raw E9")
        if host not in raw:
            errors.append(f"{name}: no contiene la IPv4 destino del receptor")
        if ps4 in raw:
            errors.append(f"{name}: contiene la IPv4 de PS4, posible destino equivocado")
        notice_strings = NOTICE_COMMON + (NOTICE_FAN if name in FAN_NAMES else NOTICE_UPDATES)
        missing_notices = [item for item in notice_strings if item not in raw]
        if missing_notices:
            errors.append(f"{name}: faltan notificaciones requeridas {missing_notices!r}")
        if b"kexec" in raw.lower() or b"jailbreak" in raw.lower():
            errors.append(f"{name}: contiene literal de ruta de escalada")
        try:
            syscalls, syscall_insns = find_syscalls(raw)
        except ValueError as exc:
            errors.append(f"{name}: {exc}")
            continue
        if syscall_insns != 1:
            errors.append(f"{name}: esperaba solo el syscall dispatcher; instrucciones syscall={syscall_insns}")
        allowed = FAN_ALLOWED if name in FAN_NAMES else DISABLE_ALLOWED if name.startswith("disable_") else ENABLE_ALLOWED
        if syscalls != allowed:
            errors.append(f"{name}: syscalls {sorted(syscalls)}; esperado {sorted(allowed)}")
        set_ioctl = bytes.fromhex("07 8f 1c c0") in raw
        get_ioctl = bytes.fromhex("08 8f 1c c0") in raw
        if name in FAN_NAMES and not (set_ioctl and get_ioctl):
            errors.append(f"{name}: faltan constantes ICC GET/SET")
        if name not in FAN_NAMES and (set_ioctl or get_ioctl):
            errors.append(f"{name}: contiene ioctl ICC no permitido")
        print(f"{name}: bytes={len(raw)} sha256={digest} syscalls={sorted(syscalls)}")

    if errors:
        for error in errors:
            print("FAIL:", error)
        raise SystemExit(1)
    print("PASS: 10 BIN auditados; notificaciones presentes en todos, grados UTF-8 en fan, sin syscall 11/escalada, SET ICC solo en fan y política de syscalls por función cumplida")


if __name__ == "__main__":
    main()

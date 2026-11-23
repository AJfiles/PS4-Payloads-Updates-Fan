#!/usr/bin/env python3
"""Audita estáticamente con Capstone los 3 nuevos binarios v2."""
from __future__ import annotations

import argparse
import hashlib
import ipaddress
import struct
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_OP_IMM, X86_REG_RIP
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
BIN_DIR = ROOT / "payloads" / "build_v2"
ELF_DIR = ROOT / "work" / "v2-payload-build"

COMMON_SYSCALLS = {585, 591, 594}
NOOP_ALLOWED = COMMON_SYSCALLS
DISABLE_ALLOWED = COMMON_SYSCALLS | {5, 6, 10, 60, 136, 190, 272}
ENABLE_ALLOWED = COMMON_SYSCALLS | {5, 6, 60, 136, 137, 190, 272}

TARGETS = [
    ("write_verify_noop.bin", NOOP_ALLOWED, [
        b"sceKernelSendNotificationRequest",
        b"Write Verify No-Op: PASS",
        b"Write Verify No-Op: FAIL",
    ]),
    ("disable_updates_full.bin", DISABLE_ALLOWED, [
        b"sceKernelSendNotificationRequest",
        b"Updates: 3/3 aplicado",
        b"Updates: 1/3 - registro no disponible",
        b"Updates: 1/3 - auto_download fallo",
        b"Updates: 2/3 - NP_env fallo",
        b"Updates: 0/3 - fallo en filesystem",
    ]),
    ("enable_updates_full.bin", ENABLE_ALLOWED, [
        b"sceKernelSendNotificationRequest",
        b"Updates: 3/3 restaurado",
        b"Updates: 1/3 - registro no disponible",
        b"Updates: 1/3 - auto_download fallo",
        b"Updates: 2/3 - NP_env fallo",
        b"Updates: 0/3 - fallo al retirar",
    ]),
]

FORBIDDEN_KEYS = [
    0x0780B007,
    0x0781B007,
]


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


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


def audit_binary(name: str, allowed_syscalls: set[int], notice_strings: list[bytes],
                 report_ip: bytes, ps4_ip: bytes, old_ip: bytes) -> list[str]:
    bin_path = BIN_DIR / name
    elf_path = ELF_DIR / f"{name}.elf"
    errors: list[str] = []

    if not bin_path.is_file():
        return [f"{name}: archivo BIN ausente"]

    raw = bin_path.read_bytes()
    digest = sha256(raw)

    # 1. Entrada 0xE9
    if not raw or raw[0] != 0xE9:
        errors.append(f"{name}: el punto de entrada no es 0xE9 (rel32 jmp)")

    # 2. IP del receptor
    if report_ip not in raw:
        errors.append(f"{name}: falta la IP del receptor ({report_ip.decode('ascii')})")
    if ps4_ip in raw:
        errors.append(f"{name}: contiene la IP de la PS4 ({ps4_ip.decode('ascii')})")
    if old_ip in raw:
        errors.append(f"{name}: contiene la IP obsoleta ({old_ip.decode('ascii')})")

    # 3. Sin kexec, sin jailbreak
    if b"kexec" in raw.lower() or b"jailbreak" in raw.lower():
        errors.append(f"{name}: contiene texto o stub de kexec/jailbreak")

    # 4. Sin constantes prohibidas en bytes raw ni en operandos inmediatos
    for fk in FORBIDDEN_KEYS:
        fk_bytes = struct.pack("<I", fk)
        if fk_bytes in raw:
            errors.append(f"{name}: contiene la constante prohibida 0x{fk:08X} en bytes crudos")

    # 5. Notificaciones requeridas
    missing_notices = [s for s in notice_strings if s not in raw]
    if missing_notices:
        errors.append(f"{name}: faltan cadenas de notificación: {missing_notices!r}")

    # 6. Syscalls permitidas
    try:
        syscalls, syscall_count = find_syscalls(raw)
        if syscall_count != 1:
            errors.append(f"{name}: se encontraron {syscall_count} instrucciones syscall directas (esperada solo 1 dispatcher)")
        if syscalls != allowed_syscalls:
            errors.append(f"{name}: syscalls {sorted(syscalls)} difieren de las permitidas {sorted(allowed_syscalls)}")
    except ValueError as exc:
        errors.append(f"{name}: {exc}")

    # 7. Desensamblado y verificación PIC (sin operandos de memoria absolutos)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    absolute_mem: list[str] = []
    forbidden_imms: list[str] = []

    for ins in md.disasm(raw, 0):
        for op in ins.operands:
            if op.type == X86_OP_MEM:
                mem = op.mem
                if mem.base == 0 and mem.index == 0:
                    absolute_mem.append(f"0x{ins.address:x}: {ins.mnemonic} {ins.op_str}")
            elif op.type == X86_OP_IMM:
                val = op.imm & 0xFFFFFFFF
                if 0x0780B000 <= val <= 0x0790B000:
                    forbidden_imms.append(f"0x{ins.address:x}: 0x{val:08x} in '{ins.mnemonic} {ins.op_str}'")

    if absolute_mem:
        errors.append(f"{name}: se detectaron operandos de memoria absoluta ({len(absolute_mem)}): {absolute_mem[:5]}")
    if forbidden_imms:
        errors.append(f"{name}: se detectaron inmediatos en rango de usuario prohibido: {forbidden_imms}")

    # 8. ELF intermedio: comprobar relocaciones
    if elf_path.is_file():
        with elf_path.open("rb") as stream:
            elf = ELFFile(stream)
            relocs = []
            for section in elf.iter_sections():
                if section["sh_type"] in ("SHT_RELA", "SHT_REL"):
                    relocs.extend(section.iter_relocations())
            if relocs:
                errors.append(f"{name}.elf: contiene {len(relocs)} relocalizaciones pendientes")

    print(f"{name}: bytes={len(raw)} sha256={digest} syscalls={sorted(allowed_syscalls)} errors={len(errors)}")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report-host", default="<PS4_IP>")
    parser.add_argument("--ps4-ip", default="<PS4_IP>")
    parser.add_argument("--old-ip", default="<PS4_IP>")
    args = parser.parse_args()

    report_ip = args.report_host.encode("ascii")
    ps4_ip = args.ps4_ip.encode("ascii")
    old_ip = args.old_ip.encode("ascii")

    all_errors: list[str] = []
    print("=== AUDITORÍA ESTÁTICA CAPSTONE (BINARIOS V2) ===")
    for name, allowed_syscalls, notices in TARGETS:
        errs = audit_binary(name, allowed_syscalls, notices, report_ip, ps4_ip, old_ip)
        all_errors.extend(errs)

    if all_errors:
        print("\nFALLOS DE AUDITORÍA:")
        for err in all_errors:
            print(f"  [ERROR] {err}")
        raise SystemExit(1)
    else:
        print("\nAUDITORÍA CAPSTONE: 100% PASS")
        print("- Punto de entrada 0xE9 en los 3 binarios")
        print("- Sin kexec, sin jailbreak, sin syscall 11")
        print(f"- Receptor incrustado verificado: {args.report_host}")
        print(f"- Sin referencias a IPs prohibidas ({args.ps4_ip}, {args.old_ip})")
        print("- Claves prohibidas (0x0780B007, rango usuario) ausentes en bytes crudos e instrucciones")
        print("- Sin operandos de memoria absolutos (PIC limpio, Regla 12.3)")
        print("- Polos de syscalls exactos y aislados por función")


if __name__ == "__main__":
    main()

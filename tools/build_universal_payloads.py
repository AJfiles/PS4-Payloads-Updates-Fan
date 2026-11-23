#!/usr/bin/env python3
"""Build reproducible candidates for fan control and update sentinels."""
from __future__ import annotations

import argparse
import hashlib
import ipaddress
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ZIG = ROOT / "research" / "toolchains" / "zig-0.13.0" / "zig-windows-x86_64-0.13.0" / "zig.exe"
SDK = ROOT / "research" / "sources" / "ps4-payload-sdk" / "libPS4"
WORK = ROOT / "work" / "universal-payload-build"
OUT = ROOT / "payloads" / "build"
FLAGS = [
    "-target", "x86_64-freestanding-none", "-Os", "-std=c11",
    "-ffunction-sections", "-fdata-sections", "-fno-builtin",
    "-fno-stack-protector", "-nostartfiles", "-nostdlib", "-Wall", "-Wextra",
    "-masm=intel", "-march=btver2", "-mtune=btver2", "-m64", "-mabi=sysv",
    "-mcmodel=small", "-fpie", "-fPIC",
]
FAN_VALUES = (50, 55, 60, 65, 70, 75, 80, 79)
FAN_NAMES = {79: "fan_default_universal.bin"}
for _v in (50, 55, 60, 65, 70, 75, 80):
    FAN_NAMES[_v] = f"fan_threshold_{_v}_universal.bin"


def run(args: list[str], cwd: Path) -> None:
    proc = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if proc.returncode:
        raise RuntimeError(f"Falló ({proc.returncode}): {' '.join(args)}\n{proc.stdout}\n{proc.stderr}")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_obj(zig: Path, src: Path, obj: Path, include: str,
                report_host: str, report_port: int, extra: list[str]) -> None:
    run([str(zig), "cc", *FLAGS, include,
         f'-DREPORT_HOST="{report_host}"', f"-DREPORT_PORT={report_port}",
         *extra, "-c", str(src), "-o", str(obj)], ROOT)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report-host", required=True, help="PC IPv4 destino para el log TCP")
    parser.add_argument("--report-port", type=int, default=9090)
    parser.add_argument("--ps4-ip", default="<PS4_IP>", help="rechaza usar la PS4 como destino de log")
    parser.add_argument("--only-updates", action="store_true",
                        help="reconstruye solo disable/enable y conserva los 8 BIN fan del manifiesto")
    args = parser.parse_args()
    try:
        report_host = str(ipaddress.IPv4Address(args.report_host))
        ps4_ip = str(ipaddress.IPv4Address(args.ps4_ip))
    except ipaddress.AddressValueError as exc:
        parser.error(f"IPv4 inválida: {exc}")
    if report_host == ps4_ip:
        parser.error("--report-host coincide con --ps4-ip; abortado para no enviar el log al BinLoader")
    if not 1 <= args.report_port <= 65535:
        parser.error("--report-port debe estar entre 1 y 65535")
    if not ZIG.is_file() or not SDK.is_dir():
        raise SystemExit("Falta Zig 0.13.0 o el checkout fijado de ps4-payload-sdk")

    # La carpeta de trabajo es propia; no toca el checkout original del SDK.
    if WORK.exists():
        shutil.rmtree(WORK)
    WORK.mkdir(parents=True)
    OUT.mkdir(parents=True, exist_ok=True)
    copied = WORK / "libPS4"
    shutil.copytree(SDK, copied)

    syscall_h = copied / "include" / "syscall.h"
    syscall_h.write_text(syscall_h.read_text(encoding="utf-8").replace("movq rax,", "mov rax,"), encoding="utf-8")
    syscall_s = copied / "source" / "syscall.s"
    asm = syscall_s.read_text(encoding="utf-8").replace("movq rax,", "mov rax,").replace("movq rdx,", "mov rdx,")
    generic = ".globl syscall\nsyscall:\n\txor rax, rax\n\n"
    if asm.count(generic) != 1:
        raise SystemExit("No se identificó de forma única el dispatcher genérico del SDK")
    syscall_s.write_text(asm.replace(generic, "", 1), encoding="utf-8")
    crt0 = WORK / "crt0.s"
    crt0.write_text((copied / "crt0.s").read_text(encoding="utf-8").replace(
        ".text", '.section .text.start,"ax"', 1), encoding="utf-8")
    include = "-I" + str(copied / "include")

    common_sources = [ROOT / "payloads" / "payload_runtime.c", ROOT / "payloads" / "kernel_syscalls.c"]
    variants: list[tuple[str, Path, Path, list[str]]] = []
    for target in FAN_VALUES:
        variants.append((FAN_NAMES[target], ROOT / "payloads" / "fan_control.c",
                         ROOT / "payloads" / "fan_syscalls.c", [f"-DFAN_TARGET={target}"]))
    variants.extend([
        ("disable_updates_universal.bin", ROOT / "payloads" / "updates_control.c",
         ROOT / "payloads" / "update_syscalls.c", ["-DUPDATE_DISABLE=1"]),
        ("enable_updates_universal.bin", ROOT / "payloads" / "updates_control.c",
         ROOT / "payloads" / "enable_syscalls.c", ["-DUPDATE_ENABLE=1"]),
    ])

    if args.only_updates:
        variants = [item for item in variants if item[0] in {
            "disable_updates_universal.bin", "enable_updates_universal.bin"}]

    sums = OUT / "SHA256SUMS.txt"
    manifest_map: dict[str, str] = {}
    if sums.is_file():
        for line in sums.read_text(encoding="ascii").splitlines():
            fields = line.split()
            if len(fields) == 2:
                manifest_map[fields[1]] = fields[0].lower()
    fan_hashes_before: dict[str, str] = {}
    if args.only_updates:
        for filename in FAN_NAMES.values():
            fan_path = OUT / filename
            expected = manifest_map.get(filename)
            if not fan_path.is_file() or not expected:
                raise SystemExit(f"No se puede preservar {filename}: BIN o hash de manifiesto ausente")
            actual = sha256(fan_path)
            if actual != expected:
                raise SystemExit(f"No se puede preservar {filename}: hash previo no coincide con manifiesto")
            fan_hashes_before[filename] = actual

    for name, action_src, syscall_src, macros in variants:
        obj_paths: list[Path] = []
        srcs = [*common_sources, action_src, syscall_src]
        for idx, src in enumerate(srcs):
            obj = WORK / f"{name}.{idx}.o"
            compile_obj(ZIG, src, obj, include, report_host, args.report_port,
                        macros if src == action_src else [])
            obj_paths.append(obj)
        elf = WORK / (name + ".elf")
        out_bin = OUT / name
        run([str(ZIG), "cc", *FLAGS,
             "-Wl,-T," + str(copied / "linker.x"), "-Wl,--build-id=none", "-Wl,--gc-sections",
             str(crt0), str(copied / "source" / "syscall.s"), *map(str, obj_paths), "-o", str(elf)], ROOT)
        run([str(ZIG), "objcopy", "-O", "binary", str(elf), str(out_bin)], ROOT)
        blob = out_bin.read_bytes()
        if not blob or blob[0] != 0xE9:
            raise SystemExit(f"{name}: no empieza con el salto raw esperado")
        if report_host.encode("ascii") not in blob:
            raise SystemExit(f"{name}: no contiene el destino TCP esperado")
        digest = sha256(out_bin)
        print(f"{name}: bytes={len(blob)} sha256={digest} report={report_host}:{args.report_port}")
        manifest_map[name] = digest

    if args.only_updates:
        changed_fan = [name for name, digest in fan_hashes_before.items()
                       if sha256(OUT / name) != digest]
        if changed_fan:
            raise SystemExit("El build tocó BIN fan fuera de alcance: " + ", ".join(changed_fan))
        missing_fan = [name for name in FAN_NAMES.values() if name not in manifest_map]
        if missing_fan:
            raise SystemExit("No se puede conservar el manifiesto: faltan hashes fan " + ", ".join(missing_fan))
        manifest_names = [FAN_NAMES[v] for v in FAN_VALUES] + [
            "disable_updates_universal.bin", "enable_updates_universal.bin"]
    else:
        manifest_names = [name for name, *_ in variants]
    sums.write_text("\n".join(f"{manifest_map[name]}  {name}" for name in manifest_names) + "\n",
                    encoding="ascii")
    print(f"manifest={sums}")


if __name__ == "__main__":
    main()

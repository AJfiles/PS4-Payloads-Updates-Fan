#!/usr/bin/env python3
"""Build the v2 binaries (write_verify_noop, disable_updates_full, enable_updates_full)."""
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
WORK = ROOT / "work" / "v2-payload-build"
OUT = ROOT / "payloads" / "build_v2"
FLAGS = [
    "-target", "x86_64-freestanding-none", "-Os", "-std=c11",
    "-ffunction-sections", "-fdata-sections", "-fno-builtin",
    "-fno-stack-protector", "-nostartfiles", "-nostdlib", "-Wall", "-Wextra",
    "-masm=intel", "-march=btver2", "-mtune=btver2", "-m64", "-mabi=sysv",
    "-mcmodel=small", "-fpie", "-fPIC",
]


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

    common_runtime = ROOT / "payloads" / "payload_runtime.c"
    kernel_syscalls = ROOT / "payloads" / "kernel_syscalls.c"
    update_syscalls = ROOT / "payloads" / "update_syscalls.c"
    enable_syscalls = ROOT / "payloads" / "enable_syscalls.c"

    # Definición de los 3 binarios
    targets: list[tuple[str, Path, list[Path]]] = [
        ("write_verify_noop.bin",
         ROOT / "payloads" / "write_verify_noop.c",
         [common_runtime, kernel_syscalls]),
        ("disable_updates_full.bin",
         ROOT / "payloads" / "disable_updates_full.c",
         [common_runtime, kernel_syscalls, update_syscalls]),
        ("enable_updates_full.bin",
         ROOT / "payloads" / "enable_updates_full.c",
         [common_runtime, kernel_syscalls, enable_syscalls]),
    ]

    manifest_map: dict[str, str] = {}
    for name, action_src, extra_srcs in targets:
        obj_paths: list[Path] = []
        all_srcs = [*extra_srcs, action_src]
        for idx, src in enumerate(all_srcs):
            obj = WORK / f"{name}.{idx}.o"
            compile_obj(ZIG, src, obj, include, report_host, args.report_port, [])
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

    sums = OUT / "SHA256SUMS.txt"
    sums.write_text("\n".join(f"{manifest_map[name]}  {name}" for name, _, _ in targets) + "\n",
                    encoding="ascii")
    print(f"manifest={sums}")


if __name__ == "__main__":
    main()

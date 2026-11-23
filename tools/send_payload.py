#!/usr/bin/env python3
"""Envía un BIN raw al BinLoader GoldHEN de una PS4 por TCP."""
from __future__ import annotations

import argparse
import ipaddress
import socket
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ps4-ip", required=True, help="IPv4 de la PS4")
    parser.add_argument("--port", type=int, default=9090, help="puerto BinLoader (default 9090)")
    parser.add_argument("--bin", required=True, type=Path, dest="payload",
                        help="ruta al BIN raw que se enviará")
    parser.add_argument("--timeout", type=float, default=10,
                        help="timeout de conexión y envío en segundos")
    args = parser.parse_args()
    try:
        ps4_ip = str(ipaddress.IPv4Address(args.ps4_ip))
    except ipaddress.AddressValueError as exc:
        parser.error(f"--ps4-ip debe ser una IPv4: {exc}")
    if not 1 <= args.port <= 65535:
        parser.error("--port debe estar entre 1 y 65535")
    if args.timeout <= 0:
        parser.error("--timeout debe ser mayor que cero")
    try:
        data = args.payload.read_bytes()
    except OSError as exc:
        parser.error(f"no se pudo leer el BIN: {exc}")
    if not data:
        parser.error("el BIN está vacío")

    try:
        with socket.create_connection((ps4_ip, args.port), timeout=args.timeout) as sock:
            sock.settimeout(args.timeout)
            sock.sendall(data)
            sock.shutdown(socket.SHUT_WR)
        print(f"Enviados {len(data)} bytes a {ps4_ip}:{args.port}.")
        print("Esto confirma el envío TCP, no que el payload terminara; espera el informe del receptor.")
        return 0
    except (OSError, TimeoutError) as exc:
        print(f"No se pudo enviar a {ps4_ip}:{args.port}: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

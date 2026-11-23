#!/usr/bin/env python3
"""Recibe por TCP el informe de goldhen_readonly_probe, solo imprime stdout."""
from __future__ import annotations

import argparse
import socket
import sys

DEFAULT_HOST = "0.0.0.0"
DEFAULT_PORT = 9090
MAX_REPORT = 16 * 1024


def receive_one(conn: socket.socket) -> bytes:
    chunks: list[bytes] = []
    total = 0
    while True:
        block = conn.recv(2048)
        if not block:
            break
        total += len(block)
        if total > MAX_REPORT:
            raise ValueError(f"reporte excede el máximo de {MAX_REPORT} bytes")
        chunks.append(block)
    return b"".join(chunks)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help="dirección de bind (por defecto todas las interfaces: 0.0.0.0)")
    parser.add_argument("--port", default=DEFAULT_PORT, type=int, help="puerto TCP de escucha")
    parser.add_argument("--timeout", default=180, type=int, help="segundos para esperar una sonda")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("--port debe estar entre 1 y 65535")

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind((args.host, args.port))
        server.listen(1)
        server.settimeout(args.timeout)
        print(f"Esperando sonda en {args.host}:{args.port} (timeout {args.timeout}s)...", flush=True)
        conn, peer = server.accept()
        with conn:
            conn.settimeout(15)
            report = receive_one(conn)
        print(f"\n--- Reporte recibido desde {peer[0]}:{peer[1]} ({len(report)} bytes) ---")
        sys.stdout.write(report.decode("utf-8", errors="replace"))
        if not report.endswith(b"\n"):
            sys.stdout.write("\n")
        print("--- Fin del reporte ---", flush=True)
        return 0
    except socket.timeout:
        print("Timeout: no llegó ningún reporte.", file=sys.stderr)
        return 2
    except (OSError, ValueError) as exc:
        print(f"Error del receptor: {exc}", file=sys.stderr)
        return 1
    finally:
        server.close()


if __name__ == "__main__":
    raise SystemExit(main())

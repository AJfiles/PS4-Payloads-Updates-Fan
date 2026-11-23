#!/usr/bin/env python3
"""Corre una medida autorizada, recibe logs y guarda el diff ICC localmente."""
from __future__ import annotations

import argparse
import hashlib
import ipaddress
import queue
import re
import socket
import threading
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = "probe/build/goldhen_readonly_probe.bin"
ALLOWLIST = {
    "probe": (PROBE, "5ef18fa92d57ddf703340ad970283605f76ee6bc84b60799f28caa35488f4b20"),
    "fan_default_original": (
        "Fan 9.00/fanDefault.bin",
        "75fd871fc09c49a18b8006b8120a5e2e407196f6cb03289b949fd50419bdeffc",
    ),
    "fan60_universal": (
        "payloads/build/fan_threshold_60_universal.bin",
        "7ee35577a903899037630ad5d201dc9a03aa480dbf9b757753186375ce702ad0",
    ),
    "enable_updates": (
        "payloads/build/enable_updates_universal.bin",
        "c7cb5370041a982a54cb1cb2cc1c004d435451f5d0b376d93c372cee47e4541a",
    ),
    "disable_updates": (
        "payloads/build/disable_updates_universal.bin",
        "0a3ae44cf6a691c17432b734687a5d386bc5561298985e882a92b5f6d381db62",
    ),
}
FAN_TARGETS = {50: "fan_threshold_50_universal.bin",
               55: "fan_threshold_55_universal.bin",
               65: "fan_threshold_65_universal.bin",
               70: "fan_threshold_70_universal.bin",
               75: "fan_threshold_75_universal.bin",
               80: "fan_threshold_80_universal.bin",
               79: "fan_default_universal.bin"}
for _target, _filename in FAN_TARGETS.items():
    _manifest = (ROOT / "payloads" / "build" / "SHA256SUMS.txt").read_text(encoding="ascii")
    _hash = next(line.split()[0] for line in _manifest.splitlines()
                 if line.split()[-1] == _filename)
    ALLOWLIST[f"fan_{_target}"] = (f"payloads/build/{_filename}", _hash)
MEASURES: dict[str, list[tuple[str, bool]]] = {
    # Relectura de estado, sin acciones; la sonda informa también entradas de /update.
    "PRE_F0": [("probe", True)],
    # (allowlist key, sends_report). Original fan files are fire-and-forget.
    "D": [("fan_default_original", False), ("probe", True)],
    "E": [("fan60_universal", True), ("probe", True)],
    # F0 valida idempotencia con preflight en lectura y disable mientras los señuelos existen.
    "F0": [("probe", True), ("disable_updates", True), ("probe", True)],
    # F no introduce pausas deliberadas en la ventana enable.
    "F": [("probe", True), ("enable_updates", True), ("probe", True),
          ("disable_updates", True), ("probe", True)],
    "F_RECHECK": [("probe", True), ("enable_updates", True), ("probe", True),
                  ("disable_updates", True), ("probe", True)],
    "C": [("probe", True)],
}
for _target in FAN_TARGETS:
    _label = "TDEFAULT" if _target == 79 else f"T{_target}"
    MEASURES[_label] = [(f"fan_{_target}", True), ("probe", True)]
TABLE_RE = re.compile(
    rb"^(?:icc\.get1\.hex28|fan\.table\.after|fan\.table\.before)=([0-9a-fA-F ]{83})$",
    re.MULTILINE,
)


@dataclass
class Capture:
    index: int
    data: bytes


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_payload(key: str) -> tuple[Path, str]:
    relative, expected = ALLOWLIST[key]
    path = (ROOT / relative).resolve()
    if ROOT.resolve() not in path.parents:
        raise RuntimeError(f"Ruta fuera del proyecto: {relative}")
    actual = sha256(path)
    if actual != expected:
        raise RuntimeError(f"Hash fuera de allowlist para {relative}: {actual} != {expected}")
    return path, actual


def receive_reports(listener: socket.socket, expected_count: int,
                    out: queue.Queue[Capture | Exception], timeout: float) -> None:
    try:
        for index in range(1, expected_count + 1):
            listener.settimeout(timeout)
            conn, peer = listener.accept()
            chunks: list[bytes] = []
            with conn:
                conn.settimeout(timeout)
                while True:
                    block = conn.recv(4096)
                    if not block:
                        break
                    chunks.append(block)
            body = b"".join(chunks)
            if not body:
                raise RuntimeError(f"Conexión de informe vacía desde {peer}")
            out.put(Capture(index, body))
    except Exception as exc:  # devuelve el fallo al hilo que ejecuta la medida
        out.put(exc)
    finally:
        listener.close()


def send_to_ps4(key: str, ps4_ip: str, port: int, timeout: float,
                events: list[str]) -> None:
    path, digest = verify_payload(key)  # hash justo antes de CADA envío
    data = path.read_bytes()
    events.append(f"SEND key={key} file={path.relative_to(ROOT)} bytes={len(data)} sha256={digest}")
    with socket.create_connection((ps4_ip, port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(data)
        sock.shutdown(socket.SHUT_WR)


def get_report(reports: queue.Queue[Capture | Exception], timeout: float) -> Capture:
    result = reports.get(timeout=timeout)
    if isinstance(result, Exception):
        raise result
    return result


def parse_table(text: str) -> bytes | None:
    matches = TABLE_RE.findall(text.encode("utf-8"))
    if not matches:
        return None
    raw = re.sub(rb"\s+", b" ", matches[-1].strip()).split(b" ")
    try:
        value = bytes(int(byte, 16) for byte in raw)
    except ValueError:
        return None
    return value if len(value) == 28 else None


def parse_tagged_table(text: str, key: str) -> bytes | None:
    match = re.search(rf"^{re.escape(key)}=([0-9a-fA-F ]{{83}})$", text, re.MULTILINE)
    if not match:
        return None
    try:
        value = bytes(int(byte, 16) for byte in match.group(1).split())
    except ValueError:
        return None
    return value if len(value) == 28 else None


def format_diff(current: bytes, reference: bytes, label: str) -> str:
    changed = [f"0x{i:02x}:{reference[i]:02x}->{current[i]:02x}"
               for i in range(28) if current[i] != reference[i]]
    return f"diff.{label}.changed={len(changed)}/28\n" + \
        f"diff.{label}.bytes=" + (", ".join(changed) if changed else "none") + "\n"


def probe_route_state(text: str, key: str) -> str:
    """Clasifica una ruta con metadatos de solo lectura de la sonda."""
    rc = re.search(rf"^{re.escape(key)}\.stat\.rc=(-?\d+)$", text, re.MULTILINE)
    if not rc:
        return "unknown"
    if int(rc.group(1)) != 0:
        err = re.search(rf"^{re.escape(key)}\.errno=(\d+)$", text, re.MULTILINE)
        return "missing" if err and int(err.group(1)) == 2 else "error"
    kind = re.search(rf"^{re.escape(key)}\.type=([^\r\n]+)$", text, re.MULTILINE)
    perms = re.search(rf"^{re.escape(key)}\.permissions_octal=([0-7]+)$", text, re.MULTILINE)
    uid = re.search(rf"^{re.escape(key)}\.uid=(\d+)$", text, re.MULTILINE)
    if kind and kind.group(1) == "file":
        return "file"
    if kind and kind.group(1) == "directory":
        count = re.search(rf"^{re.escape(key)}\.entries\.count=(\d+)$", text, re.MULTILINE)
        status = re.search(rf"^{re.escape(key)}\.entries\.status=([^\r\n]+)$", text, re.MULTILINE)
        if not count or not status or status.group(1) != "OK":
            return "directory_unverified"
        return "empty_directory" if count.group(1) == "0" else "nonempty_directory"
    return "unsafe_type"


def has_line(text: str, line: str) -> bool:
    return line in text.splitlines()


def read_previous(label: str) -> list[tuple[str, bytes]]:
    prior_map = {"PRE_F0": [], "D": ["B"], "E": ["D", "B"], "F0": ["C"], "F": ["C", "E"], "F_RECHECK": [], "C": ["E"],
                 "T50": ["C"], "T55": ["T50"], "T65": ["T55"], "T70": ["T65"],
                 "T75": ["T70"], "T80": ["T75"], "TDEFAULT": ["T80"]}
    prior = prior_map[label]
    results: list[tuple[str, bytes]] = []
    for name in prior:
        path = ROOT / "analysis" / f"probe_{name}.txt"
        if not path.is_file():
            continue
        table = parse_table(path.read_text(encoding="utf-8", errors="replace"))
        if table:
            results.append((name, table))
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", choices=tuple(MEASURES), required=True)
    parser.add_argument("--attempt", choices=("1", "2", "3", "4"), default="1",
                        help="E/F0: guarda una captura adicional sin sobrescribir informes previos")
    parser.add_argument("--ps4-ip", default="<PS4_IP>")
    parser.add_argument("--port", type=int, default=9090)
    parser.add_argument("--listen-host", default="0.0.0.0")
    parser.add_argument("--report-port", type=int, default=9090)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--settle-seconds", type=float, default=10)
    parser.add_argument("--confirm-auto-downloads-off", action="store_true",
                        help="confirma que Ajustes > Sistema > Descargas automáticas > Archivos de actualización del software del sistema está desactivado")
    parser.add_argument("--ack-update-risk", action="store_true",
                        help="reconoce que, aun con descargas automáticas desactivadas, puede existir riesgo residual durante enable")
    parser.add_argument("--confirm-np-noupdt", action="store_true",
                        help="para F: confirma NP environment = noupdt en Debug Settings")
    parser.add_argument("--confirm-rebooted", action="store_true",
                        help="requerido para C: confirma reinicio manual y GoldHEN recargado")
    args = parser.parse_args()
    try:
        ps4_ip = str(ipaddress.IPv4Address(args.ps4_ip))
    except ipaddress.AddressValueError as exc:
        parser.error(f"--ps4-ip no es IPv4: {exc}")
    if not (1 <= args.port <= 65535 and 1 <= args.report_port <= 65535):
        parser.error("los puertos deben estar entre 1 y 65535")
    if args.timeout <= 0 or args.settle_seconds < 0:
        parser.error("timeout debe ser positivo y settle-seconds no negativo")
    if args.label in {"F0", "F", "F_RECHECK"} and not args.confirm_auto_downloads_off:
        parser.error("F0/F requieren confirmar que las descargas automáticas de archivos de actualización del sistema están desactivadas")
    if args.label in {"F", "F_RECHECK"} and not args.ack_update_risk:
        parser.error("F requiere --ack-update-risk: el riesgo residual de actualización durante enable es bajo, pero no cero")
    if args.label in {"F", "F_RECHECK"} and not args.confirm_np_noupdt:
        parser.error("F requiere --confirm-np-noupdt: confirma que NP environment está en noupdt")
    if args.label == "C" and not args.confirm_rebooted:
        parser.error("C requiere --confirm-rebooted después del reinicio manual")
    if args.attempt != "1" and args.label not in {"E", "F0"}:
        parser.error("--attempt adicional solo se permite para E o F0")

    output_stem = {"F0": "F_idempotence", "F_RECHECK": "F_recheck"}.get(args.label, args.label)
    if args.attempt != "1":
        output_stem = f"{output_stem}{args.attempt}"
    output = ROOT / "analysis" / f"probe_{output_stem}.txt"
    if output.exists():
        parser.error(f"no se sobrescribe el informe existente: {output}")
    sequence = MEASURES[args.label]
    if args.label == "F":
        f0_report = ROOT / "analysis" / "probe_F_idempotence4.txt"
        if not f0_report.is_file() or "updates.status=DISABLED" not in f0_report.read_text(encoding="utf-8", errors="replace"):
            parser.error("F requiere un F0 exitoso en analysis/probe_F_idempotence4.txt")
    expected_reports = sum(1 for _, sends_report in sequence if sends_report)
    # En F, limitar cada espera de transporte/captura acorta la ventana en que
    # los directorios están ausentes; ante timeout se ejecuta el rollback único.
    operation_timeout = min(args.timeout, 5.0) if args.label in {"F", "F_RECHECK"} else args.timeout
    events: list[str] = []
    captures: list[Capture] = []
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        listener.bind((args.listen_host, args.report_port))
        listener.listen(expected_reports)
    except OSError as exc:
        listener.close()
        parser.error(f"no se pudo abrir receptor en {args.listen_host}:{args.report_port}: {exc}")

    report_queue: queue.Queue[Capture | Exception] = queue.Queue()
    thread = threading.Thread(target=receive_reports,
                              args=(listener, expected_reports, report_queue, operation_timeout),
                              daemon=True)
    thread.start()
    enable_may_be_active = False
    validation_errors: list[str] = []
    try:
        for position, (key, sends_report) in enumerate(sequence):
            send_to_ps4(key, ps4_ip, args.port, operation_timeout, events)
            if key == "enable_updates":
                enable_may_be_active = True
            elif key == "disable_updates":
                enable_may_be_active = False
            if not sends_report:
                events.append(f"WAIT settle_seconds={args.settle_seconds}")
                time.sleep(args.settle_seconds)
                continue
            capture = get_report(report_queue, operation_timeout)
            captures.append(capture)
            events.append(f"CAPTURE {capture.index} bytes={len(capture.data)}")
            if args.label in {"F0", "F", "F_RECHECK"} and position == 0:
                preflight_text = capture.data.decode("utf-8", errors="replace")
                allowed = {"missing", "file", "empty_directory"} if args.label == "F0" else {"empty_directory"}
                for route in ("update.pup", "update.net_temp"):
                    state = probe_route_state(preflight_text, route)
                    if state not in allowed:
                        validation_errors.append(f"preflight live abortado: {route}={state}; no se envió ninguna acción")
                if validation_errors:
                    break
            if position + 1 < len(sequence):
                if args.label not in {"F0", "F", "F_RECHECK"}:
                    events.append(f"WAIT settle_seconds={args.settle_seconds}")
                    time.sleep(args.settle_seconds)
    except Exception as exc:
        events.append(f"ERROR {type(exc).__name__}: {exc}")
        listener.close()
        thread.join(timeout=1)
        if enable_may_be_active:
            events.append("EMERGENCY: intentando una sola restauracion disable, sin reintentos")
            try:
                send_to_ps4("disable_updates", ps4_ip, args.port, args.timeout, events)
                enable_may_be_active = False
            except Exception as recovery_exc:
                events.append(f"EMERGENCY_DISABLE_FAILED {type(recovery_exc).__name__}: {recovery_exc}")
    if len(captures) < expected_reports and listener.fileno() >= 0:
        listener.close()
        thread.join(timeout=0.2)

    if args.label == "F0" and len(captures) >= 3:
        pre = captures[0].data.decode("utf-8", errors="replace")
        disable_log = captures[1].data.decode("utf-8", errors="replace")
        post = captures[2].data.decode("utf-8", errors="replace")
        if not has_line(disable_log, "updates.status=DISABLED"):
            validation_errors.append("F0: disable did not report DISABLED")
        routes = (("route.0", "update.net_temp"), ("route.1", "update.pup"))
        for route, path_key in routes:
            before_state = probe_route_state(pre, path_key)
            after_state = probe_route_state(post, path_key)
            if before_state == "empty_directory":
                expected_action = f"{route}.action=empty_directory_preserved"
            elif before_state == "missing":
                expected_action = f"{route}.action=mkdir_sentinel"
            elif before_state == "file":
                expected_action = f"{route}.action=mkdir_after_unlink"
            else:
                validation_errors.append(f"F0: unsafe initial state for {path_key}: {before_state}")
                continue
            if not has_line(disable_log, expected_action):
                validation_errors.append(f"F0: expected action missing: {expected_action}")
            if after_state != "empty_directory" or not has_line(disable_log, f"{route}.after.empty=yes"):
                validation_errors.append(f"F0: {path_key} is not a confirmed empty directory afterwards")
    if args.label in {"F", "F_RECHECK"} and len(captures) >= 5:
        pre = captures[0].data.decode("utf-8", errors="replace")
        enable_log = captures[1].data.decode("utf-8", errors="replace")
        after_enable = captures[2].data.decode("utf-8", errors="replace")
        disable_log = captures[3].data.decode("utf-8", errors="replace")
        after_disable = captures[4].data.decode("utf-8", errors="replace")
        if not all(probe_route_state(pre, route) == "empty_directory" for route in ("update.pup", "update.net_temp")):
            validation_errors.append("roundtrip preflight no confirmó ambas rutas como directorios vacíos")
        if not has_line(enable_log, "updates.status=ENABLED_FILES_NOT_TOUCHED"):
            validation_errors.append("enable no confirmó que solo retiró los directorios")
        for route in ("update.pup", "update.net_temp"):
            if probe_route_state(after_enable, route) != "missing":
                validation_errors.append(f"tras enable, {route} no aparece ausente en la sonda")
        if not has_line(disable_log, "updates.status=DISABLED"):
            validation_errors.append("disable final no informó DISABLED")
        for route, path_key in (("route.0", "update.net_temp"), ("route.1", "update.pup")):
            if not has_line(disable_log, f"{route}.after.empty=yes"):
                validation_errors.append(f"disable final no confirmo {route} vacio")
            if probe_route_state(after_disable, path_key) != "empty_directory":
                validation_errors.append(f"tras disable, {path_key} no volvio como directorio vacio")
    if args.label in ("D", "E") and captures:
        final_probe = parse_table(captures[-1].data.decode("utf-8", errors="replace"))
        previous_items = read_previous(args.label)
        reference = next((table for name, table in previous_items
                          if name == ("B" if args.label == "D" else "D")), None)
        expected_threshold = 79 if args.label == "D" else 60
        if not final_probe or not reference:
            validation_errors.append("no se pudo comparar la tabla final con la medida previa")
        else:
            expected = bytearray(reference)
            expected[5] = expected_threshold
            if bytes(expected) != final_probe:
                validation_errors.append(
                    f"tabla final no coincide con el objetivo {expected_threshold} ni conserva los otros bytes")
        if args.label == "E":
            action_log = captures[0].data.decode("utf-8", errors="replace")
            status = re.search(r"^fan\.status=([^\r\n]+)", action_log, re.MULTILINE)
            before = parse_tagged_table(action_log, "fan.table.before")
            after = parse_tagged_table(action_log, "fan.table.after")
            if not status or status.group(1) not in {"VERIFIED", "VERIFIED_SET_REPORTED_ERROR"}:
                validation_errors.append("fan universal no informó estado VERIFIED")
            if not before or not after:
                validation_errors.append("log del fan no contiene ambas tablas completas")
            else:
                expected_after = bytearray(before)
                expected_after[5] = 60
                if bytes(expected_after) != after:
                    validation_errors.append("fan universal reportó cambios fuera del byte 5")

    if args.label.startswith("T") and captures:
        target = 79 if args.label == "TDEFAULT" else int(args.label[1:])
        action_log = captures[0].data.decode("utf-8", errors="replace")
        probe_log = captures[-1].data.decode("utf-8", errors="replace")
        status = re.search(r"^fan\.status=([^\r\n]+)", action_log, re.MULTILINE)
        before = parse_tagged_table(action_log, "fan.table.before")
        after = parse_tagged_table(action_log, "fan.table.after")
        observed = parse_table(probe_log)
        if not status or status.group(1) not in {"VERIFIED", "VERIFIED_SET_REPORTED_ERROR"}:
            validation_errors.append(f"umbral {target}: fan payload no informó VERIFIED")
        if not before or not after or not observed:
            validation_errors.append(f"umbral {target}: faltan tablas completas en los logs")
        else:
            expected_after = bytearray(before)
            expected_after[5] = target
            if bytes(expected_after) != after:
                validation_errors.append(f"umbral {target}: el payload cambió bytes aparte del byte 5 o el umbral no coincide")
            if observed != after:
                validation_errors.append(f"umbral {target}: la sonda no confirmó la tabla escrita por el payload")
        if target == 80 and not has_line(action_log, "fan.warning=80_exceeds_stock_79"):
            validation_errors.append("umbral 80: falta el aviso de superar el valor original")

    blocks = [f"MEDIDA {args.label} intento {args.attempt}\nPS4={ps4_ip}:{args.port}\n"
              f"PC_LISTEN={args.listen_host}:{args.report_port}\n"]
    blocks.extend(event + "\n" for event in events)
    blocks.extend(f"VALIDATION FAIL: {error}\n" for error in validation_errors)
    for capture in captures:
        blocks.append(f"\n===== INFORME TCP {capture.index} =====\n")
        blocks.append(capture.data.decode("utf-8", errors="replace"))
        if not blocks[-1].endswith("\n"):
            blocks.append("\n")

    current = None
    for capture in reversed(captures):
        current = parse_table(capture.data.decode("utf-8", errors="replace"))
        if current:
            break
    if current:
        for name, previous in read_previous(args.label):
            blocks.append("\n" + format_diff(current, previous, name))
    output.write_text("".join(blocks), encoding="utf-8")
    print(f"Guardado: {output.relative_to(ROOT)}")
    if current:
        print("Tabla final: " + " ".join(f"{byte:02x}" for byte in current))
        for name, previous in read_previous(args.label):
            print(format_diff(current, previous, name).strip())
    if any(event.startswith("ERROR ") for event in events) or validation_errors:
        print("Medida incompleta; no reenvíes automáticamente. Revisa el archivo y detente.")
        manual_recovery_needed = any(event.startswith("EMERGENCY_DISABLE_FAILED") for event in events)
        if args.label in {"F", "F_RECHECK"} and len(captures) >= 5:
            final_probe_text = captures[4].data.decode("utf-8", errors="replace")
            final_disable_text = captures[3].data.decode("utf-8", errors="replace")
            manual_recovery_needed = manual_recovery_needed or \
                not has_line(final_disable_text, "updates.status=DISABLED") or \
                not all(has_line(final_disable_text, f"route.{i}.after.empty=yes")
                        for i in (0, 1)) or \
                not all(probe_route_state(final_probe_text, route) == "empty_directory"
                        for route in ("update.pup", "update.net_temp"))
        elif args.label in {"F", "F_RECHECK"} and any(event.startswith("ERROR ") for event in events):
            manual_recovery_needed = True
        if manual_recovery_needed:
            print("RECUPERACIÓN MANUAL: mantén desactivadas las descargas automáticas; no aceptes avisos ni envíes otros payloads. Con un administrador de archivos PS4 de confianza, inspecciona /update/PS4UPDATE.PUP y /update/PS4UPDATE.PUP.net.temp. Solo si una ruta está ausente, créala como directorio vacío; modo y propietario no son criterios. No borres archivos ni contenido. Si encuentras un archivo, contenido inesperado o no puedes confirmar ambas rutas como directorios vacíos, detente y solicita ayuda antes de continuar.")
        return 1
    print(f"Medida {args.label} completa; no se envió ningún payload fuera de su allowlist.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Write a compact startup/ELF report for the PS4 Kodi executable."""

import re
import subprocess
import sys
from pathlib import Path


def run(*command: str) -> str:
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def dynamic_value(output: str, tag: str) -> int | None:
    match = re.search(rf"\({re.escape(tag)}\)\s+(0x[0-9a-fA-F]+)", output)
    return int(match.group(1), 16) if match else None


def parse_hex(value: str) -> int:
    return int(value.removeprefix("0x"), 16)


def create_report() -> None:
    elf, report_path, nm_tool, readelf_tool = sys.argv[1:]
    elf_path = Path(elf)
    dynamic = run(readelf_tool, "-W", "-d", str(elf_path))
    headers = run(readelf_tool, "-W", "-l", str(elf_path))
    sections = run(readelf_tool, "-W", "-S", str(elf_path))
    relocations = run(readelf_tool, "-W", "-r", str(elf_path))
    symbols = run(nm_tool, "-a", "-n", "-C", str(elf_path))
    imports = run(nm_tool, "-D", "--undefined-only", "--format=posix", str(elf_path))

    init_address = dynamic_value(dynamic, "INIT_ARRAY")
    init_size = dynamic_value(dynamic, "INIT_ARRAYSZ")
    if init_address is None or init_size is None or init_size % 8:
        raise SystemExit("kodi.elf has no usable DT_INIT_ARRAY/DT_INIT_ARRAYSZ")

    section_offset = None
    for line in sections.splitlines():
        fields = line.split()
        if ".init_array" in fields:
            index = fields.index(".init_array")
            section_offset = parse_hex(fields[index + 3])
            break
    if section_offset is None:
        raise SystemExit("kodi.elf has no .init_array section")

    count = init_size // 8
    if section_offset + init_size > elf_path.stat().st_size:
        raise SystemExit("DT_INIT_ARRAY extends beyond kodi.elf")
    with elf_path.open("rb") as elf_file:
        elf_file.seek(section_offset)
        array_data = elf_file.read(init_size)
    raw_targets = [
        int.from_bytes(array_data[index * 8 : (index + 1) * 8], "little")
        for index in range(count)
    ]

    relocated_targets: dict[int, int] = {}
    for line in relocations.splitlines():
        fields = line.split()
        if len(fields) < 4 or "R_X86_64_RELATIVE" not in fields:
            continue
        try:
            offset = parse_hex(fields[0])
            addend = parse_hex(fields[-1])
        except ValueError:
            continue
        if init_address <= offset < init_address + init_size:
            relocated_targets[offset] = addend

    names_at: dict[int, list[str]] = {}
    logger_address = None
    for line in symbols.splitlines():
        fields = line.split(maxsplit=2)
        if len(fields) != 3 or not re.fullmatch(r"[0-9a-fA-F]+", fields[0]):
            continue
        address = int(fields[0], 16)
        name = fields[2]
        names_at.setdefault(address, []).append(name)
        if "PS4EarlyBootLog" in name:
            logger_address = address

    targets = [
        relocated_targets.get(init_address + index * 8, raw_targets[index])
        for index in range(count)
    ]
    logger_indices = [index for index, target in enumerate(targets) if target == logger_address]
    dynamic_imports = [
        line.split()[0]
        for line in imports.splitlines()
        if len(line.split()) >= 2 and line.split()[1] == "U"
    ]
    native_tls_reference = any(
        len(fields := line.split()) >= 2 and fields[-2] == "U" and "__tls_get_addr" in fields[-1]
        for line in symbols.splitlines()
    )

    lines = [
        f"ELF: {elf_path.name}",
        f"DT_INIT_ARRAY: {init_address:#x}",
        f"DT_INIT_ARRAYSZ: {init_size} bytes ({count} entries)",
        f"PS4EarlyBootLog: {logger_address:#x}" if logger_address is not None else "PS4EarlyBootLog: missing",
        f"PS4EarlyBootLog constructor index: {logger_indices[0]}" if logger_indices else "PS4EarlyBootLog is not in DT_INIT_ARRAY",
        f"native PT_TLS segment: {'yes' if any(line.lstrip().startswith('TLS ') for line in headers.splitlines()) else 'no'}",
        f"strong undefined dynamic imports: {len(set(dynamic_imports))}",
        f"__tls_get_addr reference: {'yes' if native_tls_reference else 'no'}",
        "First 64 constructor targets:",
    ]
    for index, target in enumerate(targets[:64]):
        labels = names_at.get(target, [])
        label = ", ".join(labels[:3]) if labels else "(no exact symbol)"
        lines.append(f"  [{index:04d}] {target:#x} {label}")

    output = Path(report_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote Kodi ELF startup report: {output}")
    print("\n".join(lines[:9]))


def main() -> None:
    report_path = Path(sys.argv[2])
    try:
        create_report()
    except BaseException as error:
        report_path.parent.mkdir(parents=True, exist_ok=True)
        detail = getattr(error, "stderr", None)
        message = f"Startup ELF audit failed: {type(error).__name__}: {error}\n"
        if detail:
            message += detail if isinstance(detail, str) else detail.decode(errors="replace")
        report_path.write_text(message, encoding="utf-8")
        print(message, file=sys.stderr)


if __name__ == "__main__":
    main()

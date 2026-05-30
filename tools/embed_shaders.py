#!/usr/bin/env python3
"""Generate one deterministic C++ header from named SPIR-V files."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import sys


SYMBOL_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def parse_shader(value: str) -> tuple[str, Path]:
    symbol, separator, raw_path = value.partition("=")
    if not separator or not symbol or not raw_path:
        raise argparse.ArgumentTypeError("--shader must use SYMBOL=PATH")
    if not SYMBOL_PATTERN.fullmatch(symbol):
        raise argparse.ArgumentTypeError(f"invalid C++ symbol: {symbol}")
    return symbol, Path(raw_path)


def render(entries: list[tuple[str, Path]]) -> bytes:
    lines = [
        "#pragma once",
        "",
        "#include <cstddef>",
        "",
    ]

    seen: set[str] = set()
    for symbol, path in sorted(entries):
        if symbol in seen:
            raise ValueError(f"duplicate shader symbol: {symbol}")
        seen.add(symbol)

        payload = path.read_bytes()
        # SPIR-V is consumed as 32-bit words (reinterpret_cast<const uint32_t*>),
        # so the byte array must be 4-byte aligned (VKGS-010).
        lines.append(f"alignas(4) static constexpr unsigned char {symbol}[] = {{")
        for offset in range(0, len(payload), 12):
            chunk = payload[offset : offset + 12]
            encoded = ", ".join(f"0x{byte:02x}" for byte in chunk)
            lines.append(f"    {encoded},")
        lines.append("};")
        lines.append(f"static constexpr std::size_t {symbol}_len = sizeof({symbol});")
        lines.append("")

    return ("\n".join(lines) + "\n").encode("ascii")


def write_if_different(output: Path, content: bytes) -> bool:
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and output.read_bytes() == content:
        return False

    temporary = output.with_name(f"{output.name}.tmp")
    temporary.write_bytes(content)
    os.replace(temporary, output)
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--shader", required=True, action="append", type=parse_shader)
    args = parser.parse_args()

    try:
        changed = write_if_different(args.output, render(args.shader))
    except (OSError, ValueError) as error:
        print(f"embed_shaders.py: {error}", file=sys.stderr)
        return 1

    state = "updated" if changed else "unchanged"
    print(f"{args.output}: {state}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

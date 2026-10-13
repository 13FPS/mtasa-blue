#!/usr/bin/env python3
"""Read-only executable inventory for an experimental Definitive Edition port.

Successful inspection is evidence about files, never evidence of MTA compatibility.
No installed executable is loaded or executed by this utility.
"""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import struct
import sys


MAX_PE_OFFSET = 1024 * 1024
MAX_OPTIONAL_HEADER = 4096
MAX_SECTIONS = 96
MAX_IMPORT_DESCRIPTORS = 4096
MAX_IMPORT_NAME = 1024
MACHINES = {0x014C: "x86", 0x8664: "x64", 0x01C4: "ARM", 0xAA64: "ARM64"}
ENGINE_RELATIVE_PATH = Path("Gameface/Binaries/Win64/SanAndreas.exe")


class PEError(ValueError):
    """An executable cannot safely be inspected as a Portable Executable."""


class PEReader:
    def __init__(self, stream, file_size):
        self.stream = stream
        self.file_size = file_size
        self.sections = []

    def read_at(self, offset, length):
        # Check bounds before seeking: malformed local files must not cause huge reads.
        if offset < 0 or length < 0 or offset > self.file_size - length:
            raise PEError("Truncated or out-of-bounds PE data")
        self.stream.seek(offset)
        data = self.stream.read(length)
        if len(data) != length:
            raise PEError("File changed or was truncated during inspection")
        return data

    def rva_offset(self, rva, length):
        if rva < 0 or length < 0 or rva + length > 0x100000000:
            raise PEError("Invalid import RVA")
        if rva < self.headers_size and length <= self.headers_size - rva:
            return rva
        matches = []
        for virtual_address, virtual_size, raw_offset, raw_size in self.sections:
            delta = rva - virtual_address
            if 0 <= delta < max(virtual_size, raw_size):
                if length > raw_size - delta:
                    raise PEError("Import RVA has no complete file-backed data")
                matches.append(raw_offset + delta)
        if len(matches) != 1:
            raise PEError("Import RVA is unmapped or ambiguously mapped")
        return matches[0]

    def read_rva(self, rva, length):
        return self.read_at(self.rva_offset(rva, length), length)

    def read_import_name(self, rva):
        name = bytearray()
        for index in range(MAX_IMPORT_NAME):
            byte = self.read_rva(rva + index, 1)[0]
            if byte == 0:
                if not name:
                    raise PEError("Empty import DLL name")
                try:
                    return name.decode("ascii")
                except UnicodeDecodeError as exc:
                    raise PEError("Import DLL name is not ASCII") from exc
            if byte < 32 or byte == 127:
                raise PEError("Import DLL name contains control characters")
            name.append(byte)
        raise PEError("Import DLL name exceeds inspection limit")

    def imports(self, directory, delayed):
        rva, size = directory
        if rva == 0 and size == 0:
            return []
        descriptor_size = 32 if delayed else 20
        if not rva or size < descriptor_size:
            raise PEError("Invalid import directory extent")
        dlls = set()
        limit = min(size // descriptor_size, MAX_IMPORT_DESCRIPTORS)
        for index in range(limit):
            descriptor = self.read_rva(rva + index * descriptor_size, descriptor_size)
            fields = struct.unpack("<8I" if delayed else "<5I", descriptor)
            if not any(fields):
                return sorted(dlls, key=str.casefold)
            if delayed:
                if fields[0] & ~1:
                    raise PEError("Unsupported delay import attributes")
                name_rva = fields[1] if fields[0] & 1 else fields[1] - self.image_base
            else:
                name_rva = fields[3]
            if not name_rva:
                raise PEError("Import descriptor has no DLL name")
            dlls.add(self.read_import_name(name_rva))
        raise PEError("Import directory is unterminated or exceeds inspection limit")

    def parse(self):
        dos = self.read_at(0, 64)
        if dos[:2] != b"MZ":
            raise PEError("Missing MZ signature")
        pe_offset = struct.unpack_from("<I", dos, 0x3C)[0]
        if pe_offset < 64 or pe_offset > MAX_PE_OFFSET:
            raise PEError("PE header offset is invalid or exceeds inspection limit")
        coff = self.read_at(pe_offset, 24)
        if coff[:4] != b"PE\x00\x00":
            raise PEError("Missing PE signature")
        machine, section_count, timestamp, _, _, optional_size, _ = struct.unpack_from("<HHIIIHH", coff, 4)
        if section_count > MAX_SECTIONS:
            raise PEError("Section count exceeds inspection limit")
        if optional_size < 2 or optional_size > MAX_OPTIONAL_HEADER:
            raise PEError("Optional header length is invalid or exceeds inspection limit")
        optional = self.read_at(pe_offset + 24, optional_size)
        magic = struct.unpack_from("<H", optional)[0]
        if magic == 0x010B:
            pe_format, base_offset, base_format, directory_offset = "PE32", 28, "<I", 96
        elif magic == 0x020B:
            pe_format, base_offset, base_format, directory_offset = "PE32+", 24, "<Q", 112
        else:
            raise PEError("Unsupported optional header magic")
        if len(optional) < directory_offset:
            raise PEError("Truncated optional header")
        if (machine == 0x014C and magic != 0x010B) or (machine == 0x8664 and magic != 0x020B):
            raise PEError("Machine and optional header architecture disagree")
        directory_count = struct.unpack_from("<I", optional, directory_offset - 4)[0]
        if directory_count > (optional_size - directory_offset) // 8:
            raise PEError("Data directories exceed optional header length")
        self.image_base = struct.unpack_from(base_format, optional, base_offset)[0]
        self.headers_size = struct.unpack_from("<I", optional, 60)[0]
        section_offset = pe_offset + 24 + optional_size
        table = self.read_at(section_offset, section_count * 40)
        if self.headers_size < section_offset + len(table) or self.headers_size > self.file_size:
            raise PEError("SizeOfHeaders does not cover the PE headers")
        for index in range(section_count):
            virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from("<4I", table, index * 40 + 8)
            if raw_size and (raw_offset > self.file_size - raw_size or raw_offset < self.headers_size):
                raise PEError("Section raw data is truncated or overlaps the headers")
            self.sections.append((virtual_address, virtual_size, raw_offset, raw_size))
        metadata = {
            "machine": f"0x{machine:04X}",
            "architecture": MACHINES.get(machine, "unknown"),
            "pe_format": pe_format,
            "image_base": f"0x{self.image_base:X}",
            "entry_point_rva": struct.unpack_from("<I", optional, 16)[0],
            "size_of_image": struct.unpack_from("<I", optional, 56)[0],
            "timestamp": timestamp,
            "timestamp_utc": datetime.datetime.fromtimestamp(timestamp, datetime.timezone.utc).isoformat(),
        }
        # DLL names establish static imports only; they cannot prove an active renderer.
        metadata["imports"] = {"evidence": "static_import_tables_only"}
        for name, directory_index, delayed in (("direct", 1, False), ("delay", 13, True)):
            directory = (0, 0)
            if directory_count > directory_index:
                directory = struct.unpack_from("<II", optional, directory_offset + directory_index * 8)
            try:
                metadata["imports"][name] = self.imports(directory, delayed)
            except PEError as exc:
                # Useful header inventory survives an independently malformed import table.
                metadata["imports"][name] = None
                metadata["imports"][name + "_error"] = str(exc)
        return metadata


def inspect_binary(path):
    path = Path(path).resolve()
    result = {"path": str(path)}
    try:
        with path.open("rb") as stream:
            initial = os.fstat(stream.fileno())
            result["byte_count"] = initial.st_size
            try:
                result.update(PEReader(stream, initial.st_size).parse())
                import_errors = [result["imports"][name] for name in ("direct_error", "delay_error") if name in result["imports"]]
                if import_errors:
                    # Keep the usable metadata but fail required-target checks on malformed PE data.
                    result.update(status="invalid", error="Invalid import table: " + "; ".join(import_errors))
                else:
                    result["status"] = "present"
            except PEError as exc:
                result.update(status="invalid", error=str(exc))
            digest = hashlib.sha256()
            stream.seek(0)
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
            final = os.fstat(stream.fileno())
            if (initial.st_size, initial.st_mtime_ns) != (final.st_size, final.st_mtime_ns):
                result.update(status="invalid", error="File changed during inspection")
            else:
                result["sha256"] = digest.hexdigest()
    except FileNotFoundError:
        result.update(status="absent")
    except OSError as exc:
        result.update(status="unreadable", error=str(exc))
    return result


def inventory(game_dir, classic_dir=None, mta_dir=None):
    game_dir = Path(game_dir).resolve()
    binaries = {
        "definitive_engine": inspect_binary(game_dir / ENGINE_RELATIVE_PATH),
        "definitive_bootstrap": inspect_binary(game_dir / "PlayGTASanAndreas.exe"),
    }
    errors = []
    game = binaries["definitive_engine"]
    if game["status"] != "present":
        errors.append("Required Definitive Edition engine executable is missing, invalid, or unreadable")
    elif game["architecture"] != "x64" or game["pe_format"] != "PE32+":
        errors.append("Required Definitive Edition engine executable is not AMD64 PE32+")
    if classic_dir is not None:
        classic_dir = Path(classic_dir).resolve()
        for name in ("gta_sa.exe", "gta-sa.exe"):
            binaries["classic_" + name] = inspect_binary(classic_dir / name)
    if mta_dir is not None:
        mta_dir = Path(mta_dir).resolve()
        for name in ("core.dll", "game_sa.dll", "multiplayer_sa.dll", "netc.dll"):
            binaries["mta_" + name] = inspect_binary(mta_dir / "MTA" / name)
    return {
        "schema_version": 1,
        "status": "experimental_port_required",
        "multiplayer_supported": False,
        "inventory_status": "incomplete" if errors else "complete",
        "notice": "File discovery, PE architecture, and static imports do not establish MTA compatibility or an active renderer. PE timestamps are unverified header values.",
        "game_directory": str(game_dir),
        "classic_directory": str(classic_dir) if classic_dir is not None else None,
        "mta_directory": str(mta_dir) if mta_dir is not None else None,
        "binaries": binaries,
        "errors": errors,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", required=True, type=Path, help="Definitive Edition installation root")
    parser.add_argument("--classic-dir", type=Path, help="Optional classic GTA San Andreas installation root")
    parser.add_argument("--mta-dir", type=Path, help="Optional MTA installation root containing its MTA subdirectory")
    parser.add_argument("--output", type=Path, help="Optional new JSON file outside the installation directories")
    args = parser.parse_args(argv)
    if args.output is not None:
        output = args.output.resolve()
        # Enforce read-only game inspection even if an output path is accidentally mischosen.
        for installation in (args.game_dir, args.classic_dir, args.mta_dir):
            if installation is not None and output.is_relative_to(installation.resolve()):
                parser.error("--output must be outside all inspected installation directories")
        if output.suffix.lower() != ".json":
            parser.error("--output must name a .json file")
    result = inventory(args.game_dir, args.classic_dir, args.mta_dir)
    encoded = json.dumps(result, indent=2) + "\n"
    if args.output is not None:
        try:
            # Never overwrite another artifact; callers select a fresh report path explicitly.
            with output.open("x", encoding="utf-8") as stream:
                stream.write(encoded)
        except OSError as exc:
            print(f"Unable to write JSON report: {exc}", file=sys.stderr)
            return 2
    print(encoded, end="")
    return 2 if result["errors"] else 0


if __name__ == "__main__":
    sys.exit(main())

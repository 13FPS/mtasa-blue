"""Synthetic-file checks; tests never access or execute an installed game."""

import contextlib
import hashlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest

import preflight


def make_pe(pe64=True, direct=(), delayed=(), delay_uses_va=False):
    data = bytearray(0x1200)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\x00\x00"
    optional_size = 240 if pe64 else 224
    struct.pack_into("<HHIIIHH", data, 0x84, 0x8664 if pe64 else 0x014C, 1, 123456, 0, 0, optional_size, 0x22)
    optional_offset = 0x98
    struct.pack_into("<H", data, optional_offset, 0x20B if pe64 else 0x10B)
    image_base = 0x140000000 if pe64 else 0x400000
    struct.pack_into("<Q" if pe64 else "<I", data, optional_offset + (24 if pe64 else 28), image_base)
    struct.pack_into("<I", data, optional_offset + 16, 0x1100)
    struct.pack_into("<I", data, optional_offset + 56, 0x2000)
    struct.pack_into("<I", data, optional_offset + 60, 0x200)
    directories = optional_offset + (112 if pe64 else 96)
    struct.pack_into("<I", data, directories - 4, 16)
    section_offset = optional_offset + optional_size
    data[section_offset:section_offset + 8] = b".rdata\x00\x00"
    struct.pack_into("<4I", data, section_offset + 8, 0x1000, 0x1000, 0x1000, 0x200)
    next_name_offset = 0xA00
    for names, directory_index, raw_offset, descriptor_size in ((direct, 1, 0x300, 20), (delayed, 13, 0x500, 32)):
        if not names:
            continue
        struct.pack_into("<II", data, directories + directory_index * 8, raw_offset - 0x200 + 0x1000, (len(names) + 1) * descriptor_size)
        for index, name in enumerate(names):
            encoded = name.encode("ascii") + b"\x00"
            data[next_name_offset:next_name_offset + len(encoded)] = encoded
            name_rva = next_name_offset - 0x200 + 0x1000
            if directory_index == 1:
                struct.pack_into("<5I", data, raw_offset + index * descriptor_size, 0, 0, 0, name_rva, 1)
            else:
                struct.pack_into("<8I", data, raw_offset + index * descriptor_size, 0 if delay_uses_va else 1,
                                 name_rva + image_base if delay_uses_va else name_rva, 0, 1, 0, 0, 0, 0)
            next_name_offset += len(encoded)
    return data


class PEReaderTests(unittest.TestCase):
    def inspect(self, data):
        return preflight.PEReader(io.BytesIO(data), len(data)).parse()

    def test_pe32_and_pe32_plus_metadata(self):
        for pe64, architecture, pe_format, image_base in ((False, "x86", "PE32", "0x400000"), (True, "x64", "PE32+", "0x140000000")):
            with self.subTest(pe64=pe64):
                metadata = self.inspect(make_pe(pe64))
                self.assertEqual(metadata["architecture"], architecture)
                self.assertEqual(metadata["pe_format"], pe_format)
                self.assertEqual(metadata["image_base"], image_base)
                self.assertEqual(metadata["entry_point_rva"], 0x1100)
                self.assertEqual(metadata["size_of_image"], 0x2000)
                self.assertEqual(metadata["timestamp"], 123456)

    def test_static_direct_and_delay_imports(self):
        metadata = self.inspect(make_pe(direct=("KERNEL32.dll", "D3D11.dll", "KERNEL32.dll"), delayed=("DXGI.dll",)))
        self.assertEqual(metadata["imports"]["direct"], ["D3D11.dll", "KERNEL32.dll"])
        self.assertEqual(metadata["imports"]["delay"], ["DXGI.dll"])
        self.assertEqual(metadata["imports"]["evidence"], "static_import_tables_only")

    def test_legacy_pe32_delay_va(self):
        self.assertEqual(self.inspect(make_pe(False, delayed=("USER32.dll",), delay_uses_va=True))["imports"]["delay"], ["USER32.dll"])

    def test_truncated_headers_and_sections(self):
        complete = make_pe()
        for length in (0, 1, 63, 0x83, 0x97, 0x98 + 100, 0x98 + 240 + 39, len(complete) - 1):
            with self.subTest(length=length), self.assertRaises(preflight.PEError):
                self.inspect(complete[:length])

    def test_malformed_header_fields(self):
        mutations = [
            (0, b"NO"), (0x3C, struct.pack("<I", 0xFFFFFFF0)),
            (0x3C, struct.pack("<I", 2)), (0x80, b"PX\x00\x00"),
            (0x86, struct.pack("<H", preflight.MAX_SECTIONS + 1)),
            (0x94, struct.pack("<H", 1)), (0x94, struct.pack("<H", 4097)),
            (0x98, struct.pack("<H", 0x107)), (0x84, struct.pack("<H", 0x014C)),
            (0x98 + 108, struct.pack("<I", 17)),
            (0x98 + 60, struct.pack("<I", 128)),
            (0x98 + 240 + 20, struct.pack("<I", 0xFFFFF000)),
        ]
        for offset, replacement in mutations:
            data = make_pe()
            data[offset:offset + len(replacement)] = replacement
            with self.subTest(offset=offset, replacement=replacement), self.assertRaises(preflight.PEError):
                self.inspect(data)

    def test_invalid_import_tables_are_explicitly_unavailable(self):
        data = make_pe(direct=("KERNEL32.dll",))
        struct.pack_into("<I", data, 0x300 + 12, 0xFFFFF000)
        metadata = self.inspect(data)
        self.assertIsNone(metadata["imports"]["direct"])
        self.assertIn("direct_error", metadata["imports"])
        self.assertEqual(metadata["architecture"], "x64")

    def test_unterminated_import_descriptor(self):
        data = make_pe(direct=("KERNEL32.dll",))
        struct.pack_into("<I", data, 0x98 + 112 + 8 + 4, 20)
        self.assertIsNone(self.inspect(data)["imports"]["direct"])

    def test_unterminated_or_invalid_import_name(self):
        for contents in (b"A" * preflight.MAX_IMPORT_NAME, b"\x01\x00", b"\xff\x00", b"\x00"):
            data = make_pe(direct=("KERNEL32.dll",))
            data[0xA00:0xA00 + len(contents)] = contents
            with self.subTest(contents=contents[:4]):
                self.assertIsNone(self.inspect(data)["imports"]["direct"])

    def test_invalid_delay_attributes(self):
        data = make_pe(delayed=("DXGI.dll",))
        struct.pack_into("<I", data, 0x500, 2)
        self.assertIsNone(self.inspect(data)["imports"]["delay"])


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.game = self.root / "de"
        self.engine = self.game / preflight.ENGINE_RELATIVE_PATH

    def write_engine(self, data=None):
        self.engine.parent.mkdir(parents=True)
        self.engine.write_bytes(make_pe() if data is None else data)

    def run_main(self, *args):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = preflight.main(["--game-dir", str(self.game), *args])
        return code, stdout.getvalue(), stderr.getvalue()

    def test_present_engine_has_hash_and_never_claims_compatibility(self):
        data = make_pe()
        self.write_engine(data)
        result = preflight.inventory(self.game)
        engine = result["binaries"]["definitive_engine"]
        self.assertEqual(engine["sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(engine["byte_count"], len(data))
        self.assertEqual(engine["path"], str(self.engine.resolve()))
        self.assertEqual(result["inventory_status"], "complete")
        self.assertEqual(result["status"], "experimental_port_required")
        self.assertFalse(result["multiplayer_supported"])
        self.assertEqual(result["binaries"]["definitive_bootstrap"]["status"], "absent")

    def test_bootstrap_does_not_substitute_for_engine(self):
        self.game.mkdir()
        (self.game / "PlayGTASanAndreas.exe").write_bytes(make_pe())
        code, output, _ = self.run_main()
        self.assertNotEqual(code, 0)
        result = json.loads(output)
        self.assertEqual(result["binaries"]["definitive_bootstrap"]["status"], "present")
        self.assertEqual(result["binaries"]["definitive_engine"]["status"], "absent")
        self.assertFalse(result["multiplayer_supported"])

    def test_root_engine_name_is_not_accepted(self):
        self.game.mkdir()
        (self.game / "SanAndreas.exe").write_bytes(make_pe())
        self.assertNotEqual(self.run_main()[0], 0)

    def test_wrong_architecture_and_invalid_engine_fail(self):
        invalid_import = make_pe(direct=("KERNEL32.dll",))
        struct.pack_into("<I", invalid_import, 0x300 + 12, 0xFFFFF000)
        for data in (make_pe(False), b"not executable", invalid_import):
            with self.subTest(data=data[:2]):
                if not self.engine.parent.exists():
                    self.engine.parent.mkdir(parents=True)
                self.engine.write_bytes(data)
                code, output, _ = self.run_main()
                self.assertNotEqual(code, 0)
                self.assertEqual(json.loads(output)["inventory_status"], "incomplete")

    def test_optional_known_files_and_absence(self):
        self.write_engine()
        classic, mta = self.root / "classic", self.root / "client"
        classic.mkdir()
        (classic / "gta-sa.exe").write_bytes(make_pe(False))
        (mta / "MTA").mkdir(parents=True)
        (mta / "MTA/core.dll").write_bytes(make_pe(False))
        result = preflight.inventory(self.game, classic, mta)
        self.assertEqual(result["binaries"]["classic_gta_sa.exe"]["status"], "absent")
        self.assertEqual(result["binaries"]["classic_gta-sa.exe"]["architecture"], "x86")
        self.assertEqual(result["binaries"]["mta_core.dll"]["architecture"], "x86")
        self.assertEqual(result["binaries"]["mta_netc.dll"]["status"], "absent")
        self.assertEqual(result["inventory_status"], "complete")

    def test_output_is_explicit_and_no_installed_files_are_written(self):
        self.write_engine()
        before = sorted(path.relative_to(self.game) for path in self.game.rglob("*"))
        code, output, _ = self.run_main()
        self.assertEqual(code, 0)
        self.assertEqual(sorted(path.relative_to(self.game) for path in self.game.rglob("*")), before)
        report = self.root / "report.json"
        code, output, _ = self.run_main("--output", str(report))
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(report.read_text()), json.loads(output))
        self.assertNotEqual(self.run_main("--output", str(report))[0], 0)

    def test_output_inside_each_installation_is_rejected(self):
        for directory_option in (None, "--classic-dir", "--mta-dir"):
            installation = self.game if directory_option is None else self.root / directory_option[2:]
            args = ["--output", str(installation / "report.json")]
            if directory_option:
                args.extend([directory_option, str(installation)])
            with self.subTest(directory_option=directory_option), self.assertRaises(SystemExit) as raised:
                self.run_main(*args)
            self.assertEqual(raised.exception.code, 2)
            self.assertFalse((installation / "report.json").exists())

    def test_output_must_be_json(self):
        with self.assertRaises(SystemExit):
            self.run_main("--output", str(self.root / "report.exe"))


if __name__ == "__main__":
    unittest.main()

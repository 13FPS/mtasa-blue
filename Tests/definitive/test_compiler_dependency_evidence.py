"""Root-only verifier cases; no compiler, generator, or canonical writes."""
import argparse
import copy
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import uuid
from pathlib import Path


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    assert not path.exists(), "Preserve an existing fixture"
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    return path


def pin(path):
    return {"path": str(path.resolve()), "sha256": sha(path)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--helper", type=Path, default=Path(__file__).with_name("compiler_dependency_evidence.py"))
    parser.add_argument("--current-pointer", type=Path)
    parser.add_argument("--output-directory", type=Path)
    args = parser.parse_args()
    root = next(parent for parent in Path(__file__).resolve().parents
                if (parent / "AGENTS.md").is_file() and (parent / "Client/core/CCore.cpp").is_file())
    task = root / "Build/definitive-recovery/mta-current-core-evidence-batch-20261108"
    output = (args.output_directory or task / ("cases-" + uuid.uuid4().hex)).resolve()
    assert output.is_relative_to(task.resolve()) and not output.exists()
    scratch_parent = root / "Build/tmp/current-core-evidence"
    scratch = scratch_parent / ("v-" + uuid.uuid4().hex)
    helper = args.helper.resolve()
    pointer_path = (args.current_pointer or root / "Build/definitive-recovery/current-core-compiler-evidence.json").resolve()
    result = {"schema": "mta_current_core_dependency_verifier_cases_v1", "success": False,
              "failure": None, "cases": [], "helper": pin(helper), "pointer": pin(pointer_path),
              "compilerExecuted": False, "generatorExecuted": False, "canonicalChanged": False,
              "scratchRemoved": False}
    for path in (output, scratch):
        for ancestor in (path, *path.parents):
            assert not ancestor.is_symlink(), "Linked test path refused"
        path.mkdir(parents=True, exist_ok=False)
    environment = os.environ.copy()
    environment.update(TEMP=str(scratch), TMP=str(scratch), PYTHONDONTWRITEBYTECODE="1")

    def run(name, source, expected_success, mode="--verify-core"):
        target = output / (name + ".json")
        command = [sys.executable, "-I", "-B", str(helper), mode, str(source), "--output", str(target)]
        started = time.monotonic()
        process = subprocess.run(command, cwd=root, env=environment, capture_output=True,
                                 encoding="utf-8", errors="replace", timeout=60)
        (output / (name + ".stdout.log")).write_text(process.stdout, encoding="utf-8")
        (output / (name + ".stderr.log")).write_text(process.stderr, encoding="utf-8")
        assert target.is_file(), name + " did not retain a verifier result"
        data = json.loads(target.read_text(encoding="utf-8-sig"))
        result["cases"].append({"name": name, "expectedSuccess": expected_success,
                                "exitCode": process.returncode, "result": pin(target),
                                "elapsedSeconds": round(time.monotonic() - started, 3)})
        assert (process.returncode == 0) == expected_success, name + ": " + process.stderr
        assert data["success"] is expected_success, name
        if not expected_success:
            assert data["failure"], name + " hid the failure"
        return data

    try:
        pointer = json.loads(pointer_path.read_text(encoding="utf-8-sig"))
        receipt_path = Path(pointer["receipt"]["path"])
        receipt = json.loads(receipt_path.read_text(encoding="utf-8-sig"))
        actual = run("actual-current-pointer", pointer_path, True)
        assert actual["coreEvidencePointer"] == pin(pointer_path)
        assert actual["receipt"] == pointer["receipt"]
        assert actual["verifier"] == pointer["verifier"]
        assert actual["compilerSourceInputs"] == pointer["compilerSourceInputs"]
        assert actual["retainedArtifactsAuthoritative"] is True
        assert {Path(item["path"]).name.lower() for item in actual["artifacts"]} == {"core.dll", "game_de.dll", "pthread.dll"}
        run("actual-direct-retained-receipt", receipt_path, True)
        # A normal owned build restores/removes these paths. Prove this rule
        # using owned origins, without depending on today's mutable Bin bytes.
        historical = copy.deepcopy(receipt)
        old_backend = output / "fixtures/historical/game_de.dll"
        old_backend.parent.mkdir(parents=True)
        old_backend.write_bytes(b"owned historical backend bytes\n")
        for item in historical["artifacts"]:
            item["path"] = str(old_backend if Path(item["saved"]).name.lower() == "game_de.dll"
                               else output / "fixtures/historical/removed" / Path(item["saved"]).name)
        historical_path = save(output / "fixtures/historical-origins-receipt.json", historical)
        historical_pointer = copy.deepcopy(pointer)
        historical_pointer["receipt"] = pin(historical_path)
        run("retained-original-paths-are-historical",
            save(output / "fixtures/historical-origins-pointer.json", historical_pointer), True)

        def pointer_case(name, change):
            candidate = copy.deepcopy(pointer)
            change(candidate)
            path = save(output / "fixtures" / (name + ".json"), candidate)
            run(name, path, False)

        pointer_case("bad-receipt-pin", lambda item: item["receipt"].update(sha256="0" * 64))
        pointer_case("bad-verifier-pin", lambda item: item["verifier"].update(sha256="0" * 64))
        pointer_case("bad-pointer-input-count", lambda item: item.update(compilerSourceInputs=pointer["compilerSourceInputs"] + 1))
        pointer_case("bad-pointer-core-hash", lambda item: item.update(coreSha256="0" * 64))
        pointer_case("bad-pointer-backend-hash", lambda item: item.update(backendSha256="0" * 64))
        pointer_case("unsuccessful-pointer", lambda item: item.update(success=False))

        bad_input = output / "fixtures/changed.cpp"
        bad_input.write_bytes(b"owned changed source bytes\n")
        bad_header = output / "fixtures/compiled.h"
        bad_header.write_bytes(b"owned changed generated bytes\n")
        bad_library = output / "fixtures/changed.lib"
        bad_library.write_bytes(b"owned changed library bytes\n")
        bad_core = output / "fixtures/bad/core.dll"
        bad_core.parent.mkdir(parents=True)
        bad_core.write_bytes(b"owned changed artifact bytes\n")

        def receipt_case(name, change):
            candidate = copy.deepcopy(receipt)
            change(candidate)
            path = save(output / "fixtures" / (name + "-receipt.json"), candidate)
            wrapper = copy.deepcopy(pointer)
            wrapper["receipt"] = pin(path)
            wrapper_path = save(output / "fixtures" / (name + "-pointer.json"), wrapper)
            run(name, wrapper_path, False)

        def generated(item):
            return next(value for value in item["compiler_read_inputs"]["inputs"] if "compiledSnapshot" in value)

        receipt_case("changed-consumed-input", lambda item: item["compiler_read_inputs"]["inputs"][0].update(path=str(bad_input)))
        receipt_case("changed-compiled-snapshot", lambda item: generated(item).update(compiledSnapshot=str(bad_header)))
        receipt_case("unproved-generated-equivalence", lambda item: generated(item).update(canonicalEquivalent=False))
        receipt_case("changed-retained-core", lambda item: next(value for value in item["artifacts"] if Path(value["saved"]).name.lower() == "core.dll").update(saved=str(bad_core)))
        receipt_case("missing-retained-pthread", lambda item: item.update(artifacts=[value for value in item["artifacts"] if Path(value["saved"]).name.lower() != "pthread.dll"]))
        receipt_case("changed-reused-library", lambda item: item["reused_link_libraries"][0].update(path=str(bad_library)))
        receipt_case("wrong-receipt-input-count", lambda item: item["compiler_read_inputs"].update(count=pointer["compilerSourceInputs"] + 1))
        receipt_case("unsupported-retained-schema", lambda item: item.update(schema="unsupported_receipt"))

        # Legacy direct-receipt support keeps its active-original-path proof.
        # These are verifier data fixtures, never compiler/build evidence.
        legacy_directory = output / "fixtures/legacy"
        legacy_artifacts = []
        for name in ("core.dll", "pthread.dll"):
            saved = legacy_directory / "saved" / name
            active = legacy_directory / "active" / name
            for path in (saved, active):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(("owned legacy verifier fixture " + name).encode("ascii"))
            legacy_artifacts.append({"path": str(active), "saved": str(saved), "sha256": sha(saved)})
        legacy = {"schema": "mta_live_moveanim_fresh_core_build_v1", "syntheticFixtureOnly": True,
                  "success": True, "core_built": True,
                  "compiler_read_inputs": {"inputs": [pin(bad_input)]}, "artifacts": legacy_artifacts}
        legacy_path = save(legacy_directory / "receipt.json", legacy)
        legacy_result = run("legacy-direct-receipt", legacy_path, True)
        assert legacy_result["retainedArtifactsAuthoritative"] is False
        broken_legacy = copy.deepcopy(legacy)
        broken_legacy["artifacts"][0]["path"] = str(bad_core)
        run("legacy-active-path-still-required", save(legacy_directory / "bad-active.json", broken_legacy), False)

        # Exercise the existing source capture path with real UTF-16 CL syntax,
        # including classic C/RC/include extensions and excluded non-input lines.
        source_directory = output / "fixtures/trace-sources"
        source_directory.mkdir()
        suffixes = (".c", ".cpp", ".h", ".hpp", ".hxx", ".cc", ".cxx", ".inl", ".inc", ".ixx", ".rc")
        files = []
        for index in range(101):
            path = source_directory / (str(index) + suffixes[index % len(suffixes)])
            path.write_text("owned trace source " + str(index) + "\n", encoding="utf-8")
            files.append(path.resolve())
        ignored = source_directory / "ignored.txt"
        ignored.write_text("not a compiler source", encoding="utf-8")
        trace_directory = output / "fixtures/compiler-traces"
        trace_directory.mkdir()
        lines = ["^" + str(files[0]), *(str(path) for path in files), str(ignored), "relative.cpp", str(source_directory / "missing.h")]
        (trace_directory / "CL.read.1.tlog").write_text("\n".join(lines) + "\n", encoding="utf-16")
        captured = run("classic-and-current-trace-capture", trace_directory, True, "--capture-trace-directory")
        assert captured["sourceInputCount"] == len(files)
        assert {Path(item["path"]).resolve() for item in captured["inputs"]} == set(files)

        preserved = output / "actual-current-pointer.json"
        before = sha(preserved)
        process = subprocess.run([sys.executable, "-I", "-B", str(helper), "--verify-core", str(pointer_path),
                                  "--output", str(preserved)], cwd=root, env=environment, capture_output=True,
                                 encoding="utf-8", errors="replace", timeout=60)
        (output / "preserve-existing-result.stderr.log").write_text(process.stderr, encoding="utf-8")
        assert process.returncode != 0 and sha(preserved) == before
        result["cases"].append({"name": "preserve-existing-result", "expectedSuccess": False,
                                "exitCode": process.returncode, "existingBytesUnchanged": True})
        assert sha(helper) == result["helper"]["sha256"] and sha(pointer_path) == result["pointer"]["sha256"]
        result["success"] = True
    except Exception as error:
        result["failure"] = repr(error)
    finally:
        resolved = scratch.resolve()
        assert resolved.is_relative_to(scratch_parent.resolve()) and resolved != scratch_parent.resolve()
        assert not any(path.is_symlink() for path in (resolved, *resolved.parents))
        assert not any(path.is_symlink() for path in resolved.rglob("*"))
        shutil.rmtree(resolved)
        result["scratchRemoved"] = not resolved.exists()
        result["success"] = result["success"] and result["scratchRemoved"]
        save(output / "test-result.json", result)
    print(json.dumps({"success": result["success"], "cases": len(result["cases"]),
                      "receipt": str(output / "test-result.json"), "failure": result["failure"]}))
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

"""Record real compiler source inputs; task metadata is never a C++ input."""
import argparse
import hashlib
import json
import os
from pathlib import Path


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def pin(path):
    return {"path": str(Path(path).resolve()), "sha256": sha(path)}


def exact(item):
    assert sha(item["path"]) == item["sha256"], "Pinned Core evidence changed: " + item["path"]


def verify_core(source):
    source = Path(source).resolve()
    source_pin = pin(source)
    document = json.loads(source.read_text(encoding="utf-8-sig"))
    pointer = None
    if document.get("schema") == "mta_current_retained_core_compiler_evidence_v2":
        assert document["success"], "Current Core pointer is unsuccessful"
        pointer = document
        exact(pointer["receipt"])
        exact(pointer["verifier"])
        source = Path(pointer["receipt"]["path"]).resolve()
        document = json.loads(source.read_text(encoding="utf-8-sig"))

    assert document["success"] and document["core_built"], "Core compilation did not succeed"
    retained = document.get("schema") == "mta_core_idle_normal_release_x64_build_v1"
    if pointer:
        assert retained, "Current retained pointer names an unsupported receipt schema"
    if retained:
        assert document["scratch_removed"] and document["default_projects_restored"]
        assert document["canonical_languages_restored"] and document["workspace_bin_restored"]
    inputs = document["compiler_read_inputs"]["inputs"]
    assert inputs, "Actual Core compiler inputs are missing"
    if pointer:
        assert len(inputs) == pointer["compilerSourceInputs"], "Current Core input count differs"
    if "count" in document["compiler_read_inputs"]:
        assert len(inputs) == document["compiler_read_inputs"]["count"]
    for item in inputs:
        expected = item.get("canonicalSha256", item["sha256"])
        assert sha(item["path"]) == expected, "Consumed Core input changed: " + item["path"]
        if expected != item["sha256"]:
            assert "compiledSnapshot" in item, "Compiled generated-header bytes are missing"
        if "compiledSnapshot" in item:
            assert item["canonicalEquivalent"] and sha(item["compiledSnapshot"]) == item["sha256"]

    # Owned Core builds deliberately restore workspace Bin outputs and remove
    # compiler scratch. The saved bytes, rather than those original paths,
    # are the artifact authority. Direct legacy receipts retain their old proof.
    required = {"core.dll", "game_de.dll", "pthread.dll"} if retained else {"core.dll", "pthread.dll"}
    artifacts = {}
    for item in document["artifacts"]:
        name = Path(item["saved"]).name.lower()
        if name in required:
            assert name not in artifacts, "Duplicate retained Core artifact: " + name
            assert sha(item["saved"]) == item["sha256"], "Retained Core artifact changed: " + item["saved"]
            if not retained:
                assert sha(item["path"]) == item["sha256"], "Legacy Core artifact path changed: " + item["path"]
            artifacts[name] = {"path": item["saved"], "sha256": item["sha256"]}
    assert artifacts.keys() == required, "Required Core artifact set is incomplete"
    if retained:
        assert document["reused_link_libraries"], "Actual reused Core link libraries are missing"
        for item in document["reused_link_libraries"]:
            exact(item)
    if pointer:
        assert artifacts["core.dll"]["sha256"] == pointer["coreSha256"]
        assert artifacts["game_de.dll"]["sha256"] == pointer["backendSha256"]
        exact(pointer["receipt"])
        exact(pointer["verifier"])
    exact(source_pin)
    result = {"receipt": pin(source), "compilerSourceInputs": len(inputs),
              "actualConsumedInputsUnchanged": True, "artifacts": list(artifacts.values()),
              "retainedArtifactsAuthoritative": retained,
              "scope": "Actual Core compiler inputs and generated-header snapshots; retained Core/backend/pthread and reused libraries for owned builds, original paths for legacy receipts"}
    if pointer:
        result.update(coreEvidencePointer=source_pin, verifier=pointer["verifier"])
    return result


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--verify-core")
    group.add_argument("--capture-trace-directory")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    out = Path(args.output)
    assert not out.exists(), "Preserve an existing result"
    record = {"success": False, "failure": None}
    try:
        if args.verify_core:
            record.update(verify_core(args.verify_core))
        else:
            traces = sorted(Path(args.capture_trace_directory).rglob("CL.read.*.tlog"))
            assert traces, "Actual compiler read trace missing"
            suffixes = {".h", ".hpp", ".hxx", ".c", ".cpp", ".cc", ".cxx", ".inl", ".inc", ".ixx", ".rc"}
            inputs = {}
            for trace in traces:
                content = trace.read_text(encoding="utf-16")
                for line in content.splitlines():
                    for value in line.lstrip("^").split("|"):
                        path = Path(value.strip())
                        if path.is_absolute() and path.suffix.lower() in suffixes and path.is_file():
                            inputs[os.path.normcase(str(path.resolve()))] = path.resolve()
            assert len(inputs) > 100, "Incomplete normal compiler input graph"
            record.update(sourceInputCount=len(inputs),
                          inputs=[{"path": str(path), "sha256": sha(path)} for _, path in sorted(inputs.items())],
                          traces=[{"path": str(path.resolve()), "sha256": sha(path)} for path in traces],
                          scope="Existing source/header files actually read by CL; flags/project/command and disposable PCH are recorded separately")
        record["success"] = True
    except Exception as error:
        record["failure"] = repr(error)
    out.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"success": record["success"], "receipt": str(out),
                      "inputs": record.get("sourceInputCount", record.get("compilerSourceInputs")),
                      "failure": record["failure"]}))
    return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

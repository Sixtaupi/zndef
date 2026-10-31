#!/usr/bin/env python3
"""Bind Twister results and requirement references to the exact tested inputs."""

# Copyright (c) 2026 Romain Pelletant
# SPDX-License-Identifier: Apache-2.0

import argparse
import hashlib
import json
import re
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
ZTEST = re.compile(r"\bZTEST(?:_F)?\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*\{")
REFERENCE = re.compile(r"\b([a-z][a-z0-9_]+)\.(test_\w+)\b")


def without_comments(text):
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*', re.S)
    return tokens.sub(
        lambda match: "" if match.group().startswith(("/*", "//")) else match.group(), text
    )


def test_bodies():
    result = {}
    for path in sorted((ROOT / "tests").rglob("*.c")):
        if any(part.startswith("build") for part in path.relative_to(ROOT).parts):
            continue
        text = path.read_text()
        for match in ZTEST.finditer(text):
            depth = 1
            end = None
            for token in TOKEN.finditer(text, match.end()):
                if token.group() == "{":
                    depth += 1
                elif token.group() == "}":
                    depth -= 1
                    if depth == 0:
                        end = token.end()
                        break
            if end is None:
                raise ValueError(f"Unclosed test body: {path}:{match.group(2)}")
            name = f"{match.group(1)}.{match.group(2)}"
            if name in result:
                raise ValueError(f"Duplicate test identifier: {name}")
            result[name] = {
                "source": str(path.relative_to(ROOT)),
                "line": text.count("\n", 0, match.start()) + 1,
                "body": without_comments(text[match.end() : end]),
            }
    return result


def public_functions():
    functions = set()
    for path in sorted((ROOT / "include/ndef").glob("*.h")):
        text = without_comments(path.read_text())
        functions.update(re.findall(r"\b(ndef_\w+)\s*\([^;{}]*\)\s*(?:;|\{)", text))
    return sorted(functions)


def runtime_id(test):
    suite, name = test.split(".", 1)
    return f".{suite}.{name.removeprefix('test_')}"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def input_hashes():
    inputs = []
    for directory in ["include/ndef", "src", "tests", "doc"]:
        for path in sorted((ROOT / directory).rglob("*")):
            relative = path.relative_to(ROOT)
            if not path.is_file() or "evidence" in relative.parts:
                continue
            if any(part.startswith("build") and part != "build_evidence.py" for part in relative.parts):
                continue
            if path.suffix in {".c", ".h", ".py", ".sh", ".conf", ".yaml", ".md", ".pdf"} or path.name in {
                "CMakeLists.txt", "Kconfig"
            }:
                inputs.append(path)
    inputs.extend([ROOT / "CMakeLists.txt", ROOT / "Kconfig"])
    return {str(path.relative_to(ROOT)): digest(path) for path in inputs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, help="Twister JSON report")
    parser.add_argument("--matrix", type=Path, default=ROOT / "tests/CONFORMANCE.md")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--coverage-summary", type=Path)
    parser.add_argument("--snapshot", type=Path, help="Input snapshot captured before Twister")
    parser.add_argument("--capture-inputs", type=Path, help="Capture inputs and exit before running Twister")
    args = parser.parse_args()

    if args.capture_inputs:
        args.capture_inputs.parent.mkdir(parents=True, exist_ok=True)
        args.capture_inputs.write_text(json.dumps({
            "captured_at": datetime.now(timezone.utc).isoformat(),
            "inputs_sha256": input_hashes(),
        }, indent=2) + "\n")
        return
    if not all([args.report, args.output, args.snapshot]):
        parser.error("--report, --output and --snapshot are required to generate evidence")

    report = json.loads(args.report.read_text())
    snapshot = json.loads(args.snapshot.read_text())
    current_hashes = input_hashes()
    if snapshot["inputs_sha256"] != current_hashes:
        raise SystemExit("Inputs changed since the pre-run snapshot; rerun Twister")
    # Twister serializes its run date with whole-second precision.
    snapshot_time = datetime.fromisoformat(snapshot["captured_at"]).replace(microsecond=0)
    if datetime.fromisoformat(report["environment"]["run_date"]) < snapshot_time:
        raise SystemExit("Twister report predates the input snapshot; rerun Twister")
    tests = test_bodies()
    references = sorted({f"{s}.{t}" for s, t in REFERENCE.findall(args.matrix.read_text())})
    missing = sorted(set(references) - set(tests))
    if missing:
        raise SystemExit(f"Requirement matrix references undefined tests: {missing}")

    scenarios = report["testsuites"]
    errors = [s for s in scenarios if s.get("status") in {"failed", "error", "blocked"}]
    if errors:
        raise SystemExit("Twister contains failed/error/blocked configurations")

    for test, info in tests.items():
        info.pop("body")
        info["executions"] = [
            {"scenario": s["name"], "platform": s["platform"], "status": case["status"]}
            for s in scenarios
            for case in s.get("testcases", [])
            if case["identifier"].endswith(runtime_id(test))
        ]
    unexecuted = [
        test for test, info in tests.items()
        if not any(ex["status"] == "passed" for ex in info["executions"])
    ]
    if unexecuted:
        raise SystemExit(f"Tests never reported passed (build-only is insufficient): {unexecuted}")

    # This index locates direct test calls. Executable line/branch coverage is
    # reported separately; call presence alone is not evidence of correctness.
    bodies = test_bodies()
    api_index = {
        function: sorted(
            test for test, info in bodies.items()
            if re.search(r"\b" + re.escape(function) + r"\s*\(", info["body"])
        )
        for function in public_functions()
    }
    unreferenced = [name for name, callers in api_index.items() if not callers]
    if unreferenced:
        raise SystemExit(f"Public functions without a direct test call: {unreferenced}")

    payload = {
        "scope": "Executable requirements/API evidence; not NFC Forum certification.",
        "environment": report["environment"],
        "report_sha256": digest(args.report),
        "snapshot_captured_at": snapshot["captured_at"],
        "inputs_sha256": current_hashes,
        "counts": {
            "source_tests": len(tests),
            "matrix_test_references": len(references),
            "public_functions": len(api_index),
            "configurations": len(scenarios),
            "testcase_statuses": dict(Counter(
                case["status"] for s in scenarios for case in s.get("testcases", [])
            )),
        },
        "requirements_tests": references,
        "tests": tests,
        "api_direct_test_calls": api_index,
    }
    if args.coverage_summary:
        payload["coverage"] = json.loads(args.coverage_summary.read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2) + "\n")
    print(json.dumps(payload["counts"], indent=2))


if __name__ == "__main__":
    main()

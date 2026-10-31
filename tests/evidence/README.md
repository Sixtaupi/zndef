# Recorded verification evidence

The full Twister run executed **346 test cases successfully** across QEMU x86
and Cortex-M3. Ten configurations executed and passed; two additional minimal
sample configurations compiled successfully and were not executed. There were
no build warnings, failures or errors. The sources define 145 distinct ztests,
including 71 new tests derived from the five supplied specifications. Repeated
execution across configurations explains the larger executed-case count.

The [requirements matrix](../CONFORMANCE.md) references 95 distinct tests.
All 82 public functions have direct test calls. This call index is a navigation
aid; correctness evidence comes from each test's assertions and independent
expected values, with runtime code coverage reported separately.

The additional coverage run executed all 143 tests of the full library scenario
on QEMU x86. Coverage includes the library sources and its inline headers,
excluding the tests and Zephyr itself:

| Metric | Covered / total | Percentage |
|---|---|---|
| Functions, including internal helpers | 136 / 136 | 100.0% |
| Executable lines | 1658 / 1783 | 93.0% |
| Instrumented branches | 1394 / 1680 | 83.0% |

Coverage is measured for the full-feature configuration. The separate functional
run exercises configurations with validation disabled, all RTDs opaque and only
the core enabled. Those conditional builds are not merged into the coverage
percentages above. Uncovered error branches remain visible in the report;
100% function execution is not a claim that every input or branch is verified.

## Artifacts

- [conformance-evidence.json](conformance-evidence.json): pre-run snapshot time,
  SHA-256 hashes of 54 specification/source/header/test inputs, the functional
  report hash, per-test executions, API call index and coverage summary.
- [twister.json](twister.json): original functional results and run environment.
- [twister_report.xml](twister_report.xml): individual test results in JUnit format.
- [coverage-twister.json](coverage-twister.json): original instrumented-run results.
- [coverage-summary.json](coverage-summary.json): machine-readable coverage totals
  and per-file counts.
- [HTML coverage report](coverage/index.html), [Cobertura XML](coverage/coverage.xml)
  and [text coverage report](coverage/coverage.txt): executable lines and branches,
  including uncovered code.

The run used Zephyr `v4.5.0`, commit
`41d10b59730406bbab4ca9f1fbbe9e96aafb4966`, SDK `1.0.1`, GCC `14.3.0`,
QEMU `10.0.2`, west `1.5.0` and gcovr `8.6`. Exact machine timestamps and command
options are retained in the original reports. The SDK QEMU identifies its build
as `v10.0.2-20-g5828172dcf-dirty`; this string is preserved in the evidence JSON.

## Reproduce and interpret

From the project directory in a configured west workspace:

```sh
./tests/run_conformance.sh /tmp/zndef-conformance
west twister -T tests/ndef -p qemu_x86 --inline-logs \
  --coverage --coverage-tool gcovr --coverage-basedir "$PWD" \
  --coverage-formats html,xml,txt -O /tmp/zndef-conformance-coverage
```

The runner records hashes before Twister and rejects changed inputs or a report
predating that snapshot. Its generator also rejects undefined requirement-test
references, unexecuted source tests, missing API test calls and failed
configurations. Stale-report and changed-input rejection paths were exercised.

This campaign also corrected malformed UTF-16 acceptance, isolated CR/LF in Text,
language-tag syntax on decode, invalid UTF-8 percent escapes in external RTD names,
trailing data in Smart Poster payloads and invalid MIME icon classification.
The independent regression vectors remain in the test sources.

The supplied PDFs do not include the Signature, Handover, Device Information or
tag TLV specifications. Tests for those APIs establish their current contract,
not independent conformance to unavailable specifications. Application policies,
referenced RFC semantics, very large payloads and recognized chunked sub-records
inside Smart Poster/Handover have the explicit limits listed in the matrix.
No NFC radio/hardware interoperability or NFC Forum certification is claimed.

## MISRA Required corrections

The functional and coverage reports were refreshed after the corrections
reviewed in [misra-required.md](../../doc/misra-required.md). Compilation
with strict prototype warnings also passed. The original MISRA report has
not been regenerated: Cppcheck and its addon are unavailable in this
environment. These successful runtime tests do not establish that the
MISRA diagnostics have disappeared. The evidence JSON records the original
report hash and explicitly sets `analysis_rerun` to `false`.

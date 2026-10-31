# zndef

NFC Data Exchange Format (NDEF) library, packaged as an external Zephyr module.

> **Status:** draft/experimental. zndef implements a limited NDEF/RTD subset,
> not the complete specification. Smart Poster and handover subrecords fail explicitly.

## Coverage

- NDEF 1.0
- RTD 1.0
- RTD-Text 1.0
- RTD-URI 1.0
- RTD-SP 1.0
- RTD-Sig 1.0/2.0
- RTD handover 1.0
- TLV for T1, T2 and T5

## Integration

Add the module to your `west.yml`:

```yaml
manifest:
  projects:
    - name: zndef
      url: https://github.com/sixtaupi/zndef
      revision: main
      path: modules/lib/zndef
```

or list it in `EXTRA_ZEPHYR_MODULES`, then enable it:

```
CONFIG_ZNDEF=y
```

Each RTD has its own option (`CONFIG_ZNDEF_RTD_TEXT`, `_URI`, `_SP`, `_SIG`,
`_HANDOVER`, `_DI`) and so do the table encoding (`CONFIG_ZNDEF_ITEMS`) and
the TLV helpers (`CONFIG_ZNDEF_TLV`).
`CONFIG_ZNDEF_VALIDATE_RTD` enables the payload checks of
`NDEF_VALIDATE_RTD` for the enabled RTDs. All are enabled by default.

## Usage

Encode a message from a table:

```c
#include <ndef/items.h>

static const struct ndef_item msg[] = {
	NDEF_ITEM_TEXT("en", "Hello"),
	NDEF_ITEM_URI("https://zephyrproject.org"),
};

static int encode_message(uint8_t *buf, size_t size, size_t *len)
{
	return ndef_items_encode(msg, NDEF_ITEMS_COUNT(msg), buf, size, len);
}
```

For dynamic content, use `ndef_writer_init()`, append records with
`ndef_text_write()` / `ndef_uri_write()`, then call `ndef_writer_finish()`.
Check each return value.

For example, `ndef_text_write(&w, "en", "Hello")` encodes and appends a Text record.

To read a message, initialize `ndef_msg_iter_init()`, iterate with
`ndef_msg_iter_next()` (until `-ENOENT`), then parse the desired records with
`ndef_text_parse()`, `ndef_uri_parse()`, etc. Check incoming messages with
`ndef_msg_validate()`; use `ndef_msg_validate_buf()` with a separate scratch
buffer when validating fragmented RTD records.

Reassemble fragmented records with `ndef_record_reassemble()` before parsing.
Decoded fields borrow the input or scratch buffer: keep it valid while using them.
Fragmented typed subrecords inside Smart Poster or Handover return `-ENOTSUP`.

See [samples/ndef_tag](samples/ndef_tag) for a complete encode/decode example.

## Limits

- Signatures: the library encodes and decodes Signature records and hands
  over the records each signature covers (`ndef_sig_foreach_signed()`).
  Hashing and verification are left to the application (PSA Crypto, ...).
  `ndef_sig_record_data()` feeds the TYPE, ID and PAYLOAD fields; check that
  this matches the signing profile you target.
- Carrier configuration payloads (Bluetooth OOB, Wi-Fi WSC) are separate
  specifications and are not decoded; they are ordinary media records.

## Tests

```sh
./tests/run_conformance.sh /tmp/zndef-conformance
```

`CONFIG_TEST_NDEF_FUZZ_ROUNDS` sets the number of random mutation rounds.

The [conformance matrix](tests/CONFORMANCE.md) maps the local NDEF, RTD,
Text, URI and Smart Poster specifications to independent binary vectors,
boundary tests and explicit application responsibilities. Configuration
tests also exercise disabled modules and unavailable validation.
The [recorded evidence](tests/evidence/README.md) contains Twister/JUnit
results, code coverage and the SHA-256 hashes captured before execution.
This is verification of the documented codec scope, not NFC Forum certification.

GitHub Actions runs clang-format 18, builds and tests on QEMU x86 and Cortex-M3,
and analyzes the library with Cppcheck and the bundled MISRA rules on every push
and pull request. Test logs and analysis reports are retained for 14 days.
All reported Cppcheck/MISRA diagnostics fail the SAST job, including MISRA Advisory
rules; the existing Advisory findings must be resolved for that job to pass.

Local checks: `bash scripts/check-format.sh` and `sh scripts/check-misra.sh`
(the latter requires the image described in [tools/docker/cppcheck](tools/docker/cppcheck)).

Documentation (introduction and generated API):

```sh
python3 -m pip install --require-hashes -r doc/requirements.txt
python3 -m sphinx -M html doc build-doc -W --keep-going
```

Requires Doxygen and Python 3.13 or newer. Open `build-doc/html/index.html`.
See [doc/README.rst](doc/README.rst) for setup and standalone Doxygen generation.
The `Documentation` workflow builds on pushes to `master` and pull requests.
Only pushes to `master` publish to GitHub Pages. Enable **Settings > Pages > Source: GitHub Actions**
once in the repository.

# Code quality

Compliant with MISRA-C 2023 on mandatory and required rules.
Check against cppcheck misra add-on in [SAST container](tools/docker/cppcheck/).

## License

Apache-2.0

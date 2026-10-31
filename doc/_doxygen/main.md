# zndef

NFC Data Exchange Format encoding, decoding and validation for Zephyr.
The library uses caller-owned buffers and performs no dynamic allocation.

The API covers NDEF messages, typed record codecs, declarative encoding and
tag TLV framing. Decoded fields borrow their input buffers. Cryptography and
NFC transport belong to the application.

For integration and usage examples, build the Sphinx documentation as described
in `doc/README.rst`.

Introduction
============

zndef encodes, decodes and validates NFC Data Exchange Format (NDEF) messages.
It is a plain ISO C99 library integrated into Zephyr through CMake and Kconfig.
It performs no dynamic allocation: the application supplies message buffers,
writer state and, when needed, scratch space for fragmented payloads.

Scope
-----

The library implements NDEF records and messages, chunk sequences, type and ID
validation, and codecs for Text, URI, Smart Poster, Signature, Connection
Handover and Device Information records. Optional helpers encode declarative
item tables and read or write TLV blocks for Type 1, 2 and 5 tags.

This is an experimental codec library, not an NFC transport stack or a claim
of NFC Forum certification. The application provides tag access, transport,
application policy and cryptographic operations. Signature helpers expose
record data; they do not calculate hashes or verify signatures. Bluetooth OOB
and Wi-Fi WSC carrier payloads are opaque to the library.

Zephyr integration
------------------

Add the module to your west manifest:

.. code-block:: yaml

   manifest:
     projects:
       - name: zndef
         url: https://github.com/sixtaupi/zndef
         revision: main
         path: modules/lib/zndef

Alternatively, add the checkout to ``EXTRA_ZEPHYR_MODULES``. Enable the library
in your application's ``prj.conf``:

.. code-block:: text

   CONFIG_ZNDEF=y

See :external+zephyr:ref:`getting_started` for setting up a Zephyr workspace.
The following options default to enabled when ``CONFIG_ZNDEF`` is enabled.
Smart Poster also selects Text and URI support.

.. list-table:: Optional features
   :header-rows: 1
   :widths: 45 55

   * - Kconfig option
     - Feature
   * - ``CONFIG_ZNDEF_RTD_TEXT``
     - Text record codec
   * - ``CONFIG_ZNDEF_RTD_URI``
     - URI record codec
   * - ``CONFIG_ZNDEF_RTD_SP``
     - Smart Poster and nested records
   * - ``CONFIG_ZNDEF_RTD_SIG``
     - Signature record codec (versions 1.0 and 2.x layout)
   * - ``CONFIG_ZNDEF_RTD_HANDOVER``
     - Connection Handover and local subrecords
   * - ``CONFIG_ZNDEF_RTD_DI``
     - Device Information record codec
   * - ``CONFIG_ZNDEF_VALIDATE_RTD``
     - Payload validation for enabled RTDs
   * - ``CONFIG_ZNDEF_ITEMS``
     - Declarative message encoding
   * - ``CONFIG_ZNDEF_TLV``
     - Tag TLV helpers

Encode a message
----------------

With declarative items, a complete message can be described by a constant table:

.. literalinclude:: examples/encode.c
   :language: c

For dynamic content, use :c:func:`ndef_writer_init`, append records with the
RTD writers, then call :c:func:`ndef_writer_finish`. Check every return value.
The complete Zephyr application in ``samples/ndef_tag`` demonstrates encoding
and decoding.

Read and validate a message
---------------------------

Use :c:func:`ndef_msg_validate` to check a received message before consuming it.
Structural checks always run; the validation flags select additional checks:

* ``NDEF_VALIDATE_TYPES`` checks type and ID syntax.
* ``NDEF_VALIDATE_EXACT`` rejects trailing octets after the message.
* ``NDEF_VALIDATE_RTD`` validates known payloads for enabled RTD codecs.

Initialize an iterator with :c:func:`ndef_msg_iter_init`, then call
:c:func:`ndef_msg_iter_next` until it returns ``-ENOENT``. Other negative
results indicate an error. Parse each supported record with its RTD parser.

The iterator exposes logical records, including chunked records. Before passing
a chunked record to a parser requiring contiguous data, call
:c:func:`ndef_record_reassemble`. For whole-message RTD validation, use
:c:func:`ndef_msg_validate_buf` with a separate scratch buffer large enough
for the largest known fragmented RTD payload.

Decoded pointers borrow their input buffer; keep it valid while using the
records or parsed fields. Text and language fields are length-delimited and
are not necessarily NUL terminated. Reassembly changes the payload view to
reference the supplied buffer.

Unknown types and disabled RTDs remain opaque during RTD validation. A requested
check that cannot be performed returns ``-ENOTSUP``. Recognized fragmented
subrecords inside Smart Poster or Handover remain unsupported, even when a
scratch buffer is supplied for outer records.

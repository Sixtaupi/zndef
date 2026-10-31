.. _rtd__text_8h:

Text records
============

Use ``<ndef/rtd_text.h>`` for NFC Forum Text records (well-known type ``T``).
The codec handles language tags and UTF-8 or UTF-16 payloads. Enable
``CONFIG_ZNDEF_RTD_TEXT``.

Parse a complete record with :c:func:`ndef_text_parse`, or decode its payload
directly. :c:func:`ndef_text_write` appends UTF-8 text to a writer. Decoded
language and text fields are length-delimited views, not NUL-terminated strings.

.. doxygen-api:: ndef_rtd_text
   :project: zndef

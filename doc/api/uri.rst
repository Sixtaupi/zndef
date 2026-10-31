.. _rtd__uri_8h:

URI records
===========

Use ``<ndef/rtd_uri.h>`` for NFC Forum URI records (well-known type ``U``).
The codec handles the URI prefix abbreviation table. Enable
``CONFIG_ZNDEF_RTD_URI``.

Parse records with :c:func:`ndef_uri_parse` and append URI records with
:c:func:`ndef_uri_write`. Decoded suffix data borrows the input payload;
use the URI helpers when a reconstructed URI string is needed.

.. doxygen-api:: ndef_rtd_uri
   :project: zndef

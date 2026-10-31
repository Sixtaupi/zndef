.. _tlv_8h:

Tag TLV helpers
===============

Use ``<ndef/tlv.h>`` to iterate over tag TLV blocks, locate an NDEF message
or wrap an encoded message in an NDEF TLV. Enable ``CONFIG_ZNDEF_TLV``.

The helpers support NULL, Terminator, NDEF Message and other TLV framing used
by Type 1, 2 and 5 tags. They do not access tag hardware or validate the NDEF
message itself. Pass the extracted message to :c:func:`ndef_msg_validate`
when validation is required.

.. doxygen-api:: ndef_tlv
   :project: zndef

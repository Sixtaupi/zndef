.. _rtd__sp_8h:

Smart Poster records
====================

Use ``<ndef/rtd_sp.h>`` for Smart Posters containing a URI and optional titles,
action, size, media type and icons. Enable ``CONFIG_ZNDEF_RTD_SP``, which also
selects Text and URI support.

:c:func:`ndef_sp_parse` reads the outer record and exposes its nested message.
Use the accessors to retrieve titles and icons. For writing, open a nested
writer with :c:func:`ndef_sp_begin` and close it with :c:func:`ndef_sp_end`.
Recognized chunked subrecords are unsupported.

.. doxygen-api:: ndef_rtd_sp
   :project: zndef

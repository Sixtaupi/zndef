.. _rtd__di_8h:

Device Information records
==========================

Use ``<ndef/rtd_di.h>`` for Device Information records (well-known type ``Di``).
Enable ``CONFIG_ZNDEF_RTD_DI``.

:c:func:`ndef_di_parse` decodes a record and :c:func:`ndef_di_elem_get` retrieves
its individual information elements. :c:func:`ndef_di_write` encodes an element
table into a writer. Parsed elements borrow their source payload.

.. doxygen-api:: ndef_rtd_di
   :project: zndef

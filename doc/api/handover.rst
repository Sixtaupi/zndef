.. _rtd__handover_8h:

Connection Handover records
===========================

Use ``<ndef/rtd_handover.h>`` for Connection Handover request, select,
mediation, initiation and carrier records, plus local alternative-carrier,
collision-resolution and error records. Enable ``CONFIG_ZNDEF_RTD_HANDOVER``.

:c:func:`ndef_ho_parse` identifies and decodes a handover record. Use
:c:func:`ndef_ho_begin` / :c:func:`ndef_ho_end` to build its nested message.
Bluetooth OOB and Wi-Fi WSC configuration payloads remain application data;
recognized chunked handover subrecords are unsupported.

.. doxygen-api:: ndef_rtd_handover
   :project: zndef

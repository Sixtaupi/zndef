.. _rtd__sig_8h:

Signature records
=================

Use ``<ndef/rtd_sig.h>`` to decode or encode Signature records and enumerate
the records covered by a signature. Enable ``CONFIG_ZNDEF_RTD_SIG``.

:c:func:`ndef_sig_parse` exposes signature and certificate fields.
:c:func:`ndef_sig_foreach_signed` identifies the covered records and
:c:func:`ndef_sig_record_data` feeds their TYPE, ID and PAYLOAD data to an
application callback. The application supplies hashing, cryptographic
verification and the trust policy required by its signing profile.

.. doxygen-api:: ndef_rtd_sig
   :project: zndef

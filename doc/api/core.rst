.. _ndef_8h:

NDEF core
=========

Use ``<ndef/ndef.h>`` to decode physical records, iterate over logical
records, validate complete messages and encode records with a writer.
The core also handles chunked payload access, reassembly, type names and IDs.
It is available when ``CONFIG_ZNDEF`` is enabled.

Start with :c:func:`ndef_msg_validate` for received messages and
:c:func:`ndef_msg_iter_init` / :c:func:`ndef_msg_iter_next` to read their records.
For encoding, initialize a writer, append records, then finish the message.
All storage is supplied by the caller; decoded fields borrow their buffers.

.. doxygen-api:: ndef
   :project: zndef

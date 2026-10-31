.. _items_8h:

Declarative encoding
====================

Use ``<ndef/items.h>`` to describe a message as a table of items instead of
issuing individual writer calls. Item macros cover supported RTDs, raw records
and nested Smart Posters. Enable ``CONFIG_ZNDEF_ITEMS`` and the codecs used by
the table.

:c:func:`ndef_items_encode` builds a complete message into a supplied buffer.
:c:func:`ndef_items_write` appends items to an already initialized writer.

.. doxygen-api:: ndef_items
   :project: zndef

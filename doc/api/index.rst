API reference
=============

The public C API provides generic NDEF message processing, typed payload codecs
and optional helpers for declarative encoding and tag TLV framing. Include the
header for the feature you use and enable its Kconfig option where required.

Decoding returns views into caller-owned buffers. Encoding writes into the
supplied destination. Check return values and keep borrowed buffers alive for
as long as the decoded objects are used. Exact contracts and error codes are
generated directly from the public headers using Doxygen and Breathe.

Each module starts with its functions, followed by structures, typedefs,
enumerations and macros. The reference shows all public declarations, including
features that may be disabled in a particular application configuration.

.. toctree::
   :maxdepth: 1

   core
   items
   text
   uri
   smart_poster
   signature
   handover
   device_information
   tlv

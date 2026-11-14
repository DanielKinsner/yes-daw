# Clock R22 two-entry attribute list size

The independent source review checked the exact null sizing call,
typed arguments and immediate error capture. The final source checks
the three ctypes module paths and hashes the 11 selected runtime and
OS files before and after the observation.

The separately scoped native query returned FALSE and Win32 error 122,
with **72 bytes** required on this host. The input arguments match the
R22 helper's initial null sizing call. The null input does not form an
attribute list. Microsoft documents the failure on the sizing call,
while error 122 and the numeric size are observations recorded here.
[Microsoft's sizing API](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-initializeprocthreadattributelist)

Reproduce with `python -I -S -B query-size.py`. The accepted purpose
source SHA-256 is
`6E8212A32EC1D58F1E4341A60E03FC929730A13D13356218712C4573BD7058C6`.
The [raw receipt](result.json) records host versions and exact file pins.

This observation closes the local requested attribute-list byte term.
The query verified installed files and selected module paths, without
attesting loaded DLL bytes, Python cache bytes, or final-image runtime
association. Attribute population and process creation were outside
this purpose.

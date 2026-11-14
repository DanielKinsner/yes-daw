# Recover the pinned Clock controller

The official PowerShell 7.6.5 Windows x64 ZIP contains all 323 controller
files from the R18 manifest, with exact original sizes and SHA-256 hashes.
The downloaded ZIP matches the publisher's SHA-256. The staging script
also verifies the four closed directory inventories after extraction.
[PowerShell 7.6.5 release and checksums](https://github.com/PowerShell/PowerShell/releases/tag/v7.6.5)

Reproduce the R18 manifest with the
[candidate script](../2026-11-14-clock-r18-provenance/candidate.py)
and `--materialize`. Run `python -E -B stage.py <release-zip> <new-directory>`
with the unmodified Windows x64 release ZIP. The new directory has the
original closed membership. The payload was staged without executing it.

The formerly missing Python executable also matches at its installed path.
The recovered tool bytes are now available on this machine. A successor
still needs to bind the new physical paths and verify the relocated launch
profile. The frozen R18 manifest retains its original paths.

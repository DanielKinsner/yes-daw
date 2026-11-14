# Clock layout compiler recovery

The original layout compile recorded MSVC full version `194435228`. The
installed executable reports `19.44.35226.0`; the toolset directory name
`14.44.35207` is shared by both servicing builds.
[Original COFF observations](artifacts/2026-11-06-native-layout-actual-result-r1/sources/015-layout-values.json)

The Microsoft-signed Visual Studio Build Tools 17.14.40 bootstrapper created
an isolated offline layout using the `Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64`
component. The bootstrapper exited successfully. The primary tool VSIX hash
matches the catalog's `EE0BAAA3A112D255F19F6C27DCC0FF6E496949EB9F1F37BE0AC908C562A7076C`.
Its `cl.exe` matches the archived pin
`88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D`.
[Microsoft's fixed-version downloads](https://learn.microsoft.com/en-us/visualstudio/releases/2022/release-history#fixed-version-bootstrappers)

All 91 pinned files under `bin/Hostx64/x64` match members of seven packages in
that layout. The recovery uses the member's original pin to select its exact
bytes. The headless compile-only purpose can use those files in an isolated
directory with the verified original headers. This confirms the availability
of the compiler files; the moved tool path and actual include association
still need a fresh purpose review and compile receipt.
[Original file pins](artifacts/2026-11-06-native-object-layout-purpose-source-r1/sources/025-source-pins.json)

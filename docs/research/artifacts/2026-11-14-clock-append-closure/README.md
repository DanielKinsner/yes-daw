# Exact MSVC string append closure layout

Pinned MSVC 19.44.35228 [measured](result.json) the original closure passed
to `_Reallocate_grow_by` by `std::string::append(const char*, size_t)`.
The closure has size and alignment one byte. The compile exited zero and
the decoder verified one tagged export and one read-only data record.

Independent review verified the exact 361-file MSVC header mirror and
the single reversible edit to `xstring`. The wrapper returns the original
lambda type. Only the `char`, `char_traits<char>`, `allocator<char>`
specialization emits the record. The compiler reported the staged
`xstring` as one of 110 pinned or reviewed includes.

An initial staging check expected 360 files and stopped before writing
any source. The exact pinned mirror membership is 361. No target was
linked or run.

Run `python -E -B purpose.py <new-source-directory>`, then repeat it with
`--compiler <compiler-directory> --output <new-output-directory>`.

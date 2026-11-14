# Clock R22 native output destinations

This worksheet supplies the additional caller-owned output writes under
the [declared primitive convention](artifacts/2026-11-06-clock-r13-scalar-revise-r1/sources/001-20261105-clock-oracle-research-leaf-census-conventions.md).
It counts an external destination once per reached native call. It
leaves repeated internal stores and primitive CPU inside the Win32
boundary. It uses the exact R22 native files, their pinned Windows
SDK headers, and the existing call and cutoff schedules.

## Output extents and existing coverage

| Win32 site | Maximum delivered destination | Coverage |
| --- | --- | --- |
| `GetCommandLineW` | Borrowed system string | Return pointer and binding in the profile worksheet. No supplied buffer. |
| `GetModuleFileNameW` | 513 WCHAR | The profile worksheet already reserves all 513. |
| `GetFullPathNameW` and `GetVolumePathNameW` | 513 WCHAR each | Additional per validation. The `GetFullPathNameW` file-part output is null. |
| `FindFirstFileW` and `FindNextFileW` | 285 typed leaves each | Additional per reached call. The SDK record has 11 DWORD and 274 WCHAR leaves. Source aggregate initialization remains in the scan worksheet. |
| `GetFileAttributesExW` | 36 bytes | Additional. Reserve the full measured `WIN32_FILE_ATTRIBUTE_DATA` size for its LPVOID output, including a failing call. |
| `ReadFile` into request, hash or pipe storage | Requested byte count per call | Additional. Bound the requests themselves, including zero, short, refused-result and failed native outputs. |
| `ReadFile` into EOF probe | One U8 probe destination | Already in each fixed probe region. The DWORD count destination remains in the existing request/worker read boundaries. Each count is one externally delivered value under the primitive model. |
| `WriteFile` | One DWORD count | Already in the writer and publication boundaries. The buffer is input and overlapped pointer is null. |
| `GetFileSizeEx` and positional `SetFilePointerEx` | One LARGE_INTEGER value each | Existing size/position rows include the native destination separately from source initialization. The log seek to end passes a null position output. |
| `CreatePipe` | Two HANDLE destinations | Include both in each existing pipe group, apart from source initialization and the optional raw failure representation copy. At most two calls. |
| `InitializeProcThreadAttributeList` | One SIZE_T per attempt and one mutable list on the non-null attempt | Include the SIZE_T target in the existing sizing and initialization group. The null list is pure sizing. Add the full allocated extent for the non-null attempt. |
| `UpdateProcThreadAttribute` | One mutable list | Add its full allocated extent on each of the two updates. Previous-value and size pointers are null and the handle arrays are inputs. |
| `DeleteProcThreadAttributeList` | One mutable list | Add its full allocated extent. The source clears the pointer immediately, allowing at most one deletion across the two retirement passes. |
| `CreateProcessW` | Mutable 29,271-WCHAR command and four `PROCESS_INFORMATION` fields | Add the command capacity. The four fields remain with the existing native create output group. The current directory, environment, startup info and security structures at the API are input parameters. |
| `QueryInformationJobObject` | 48-byte accounting buffer | Additional. Reserve the full measured `JOBOBJECT_BASIC_ACCOUNTING_INFORMATION` extent. The return-length pointer is null and the source zero leaves remain in the cleanup worksheet. |
| `GetExitCodeProcess` | One DWORD | Already in the native exit-query row, distinct from the C++ zero initializer and the later State store. |
| `CreateThread` | Optional DWORD thread ID | Null at all reached creation groups. Passing worker context is input. The worker's later source stores remain in the worker worksheet. |
| `InterlockedExchange` and `InterlockedCompareExchange` | Up to one LONG at the pointed-to destination | Additional. The compare exchange allowance covers the branch that replaces even an equal zero. Returned values and pointer/argument evaluation remain in the existing native boundaries. |
| SRW acquire/release APIs | In/out opaque lock state | Retain their typed pointer and call boundary and the controlled lock owner. Internal synchronization state updates do not deliver a separate caller payload. |

Other inventoried providers return only values already accounted for
at their call boundary or operate on opaque native objects. No other
site passes a caller output. Heap allocation at flags zero requests
uninitialized list storage. Handle inheritance, job information sets,
flushes, and related calls do not expose output buffers. The source
still needs its native timing, ownership and closed-profile proofs.
The selected calls' public extents are specified by
[ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile),
[CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw),
[process attribute lists](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-initializeprocthreadattributelist)
and [job queries](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-queryinformationjobobject).

## Requested read bytes

Let `T(N) = N(N+1)/2`. The request file starts with at most 41,260
remaining bytes. Every continuing iteration reduces that number by
at least one. A failing or zero/invalid count read replaces a
possible continuing iteration. Reserving every requested byte
therefore costs at most `T(41,260)`.

Each identity file uses `min(remaining, 65,536)`. For a file cap
`N >= 65,536`, the same decreasing schedule yields
`65,536(N - 65,536) + T(65,536)`. Apply it separately to the
manifest and target limits of 1,048,576 and 33,554,432. The one-byte
EOF probes are separate covered scalar destinations. The pipe reads
use the full 4,096 ceiling at each of 1,071,172 reached attempts.
Each direction can finish at most one already-admitted read after
a cut. These bounds require only the requested output extent and
the existing progress/slot conditions, without exact short-read
write association. [Request and identity schedule](2026-11-14-clock-r20-identity-read-census.md), [worker read schedule](2026-11-14-clock-r18-worker-read-census.md)

## Phase composition

Each scan contributes up to `8 × (513 + 513) + 10 × 285 + 7 × 36 =
11,310` destination writes. Its tail allocation preserves the 309
terminal scans and two possible partial overlaps. Outside scans,
eight validations are prefix work and the single terminal transport
validation is tail. [Scan schedule](2026-11-14-clock-r18-stage-scan-census.md), [non-scan path schedule](2026-11-14-clock-r22-nonscan-path-meter-work.md)

The worker atomic upper is `2 × 14,402` publication comparisons,
14,404 generation tests and 1,071,176 entered read bodies. Include
14,400 acknowledgement stores and two settled-worker comparisons in
the prefix. Tail carries 16 worker-side comparisons across both
directions, 40 cleanup acknowledgement stores and two more for a
possible overlapping poll, two stop stores, and up to four settle
comparisons across actual and overlapping settlement.
[Worker schedule](2026-11-14-clock-r22-worker-thread-census.md), [receive schedule](2026-11-14-clock-r22-worker-coordinator-census.md)

Let `A` be the extent of the selected two-entry attribute list.
The source checks the sizing result against the bound value before
allocation. The existing local query observed 72 bytes, so the
numeric projection below uses `A = 72` conditionally on final host
and loader association. No value is supplied from unbound Data.
[Attribute size receipt](artifacts/2026-11-14-clock-attribute-size/result.json)

| Additional output | Prefix | Tail and overlaps |
| --- | ---: | ---: |
| Request file body | 851,214,430 | 0 |
| Manifest hash body | 66,572,025,856 | 0 |
| Target hash body | 2,196,875,804,672 | 0 |
| Directory scans | 11,862,346,470 | 3,517,410 |
| Non-scan path validations | 8,208 | 1,026 |
| Worker reads | 4,387,520,512 | 8,192 |
| Interlocked destinations | 1,128,786 | 64 |
| Process command | 29,271 | 0 |
| Attribute list | `3A = 216` | `A = 72` |
| Job query | 0 | 48 |
| Projection with qualified 72-byte list | 2,280,550,078,421 | 3,526,812 |

The per-helper projection is **2,280,553,605,233** additional
source actions under the same premises. These are source
reservations. Read caps reserve output destinations even when a
successful read returns fewer bytes. A smaller value needs its own
supported native association. All three slots use the same safe
per-helper ceiling, totaling **6,841,660,815,699**.

The [headless check](artifacts/2026-11-14-clock-native-output-store/check.py)
rechecks the exact source and SDK pins, direct-call inventory and
arithmetic. No target is executed by the check.

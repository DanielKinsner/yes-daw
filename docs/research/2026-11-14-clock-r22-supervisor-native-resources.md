# Clock R22 supervisor native resource facts

This worksheet applies to the [exact guarded R22 objects](artifacts/2026-11-14-clock-supervisor-abi/r22-object-result.json),
their [type sizes](artifacts/2026-11-14-clock-supervisor-abi/result.json),
and the [native resource boundary](artifacts/2026-11-06-clock-native-resource-boundary-r1/sources/009-audit.md).
The first layer supplies emitted object evidence. The second layer supplies
source lifetime and API evidence for resident payloads and remaining terms.

## Guarded object evidence

The [data-only reader](artifacts/2026-11-14-clock-r22-coff-resource/read-object.py)
checks all four file hashes and decodes the x64 COFF section and relocation
tables. It resolves each `.pdata` entry to its `.xdata` bytes and sums
`UWOP_ALLOC_SMALL`, `UWOP_ALLOC_LARGE` and eight bytes per
`UWOP_PUSH_NONVOL` in the version-one record. Register-save offsets address
existing storage. Chained rows here carry zero additional RSP decrement and
remain reported separately from primary frames.
[Microsoft x64 unwind format](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64)

| Translation unit | Function ranges | Chained rows | Read-only data bytes | Uninitialized data bytes |
| --- | ---: | ---: | ---: | ---: |
| Codec | 20 | 5 | 482 | 4 |
| Core | 26 | 11 | 319 | 4 |
| I/O | 37 | 23 | 190 | 0 |
| Main | 45 | 21 | 0 | 0 |
| Total | 128 | 60 | 991 | 8 |

The eight uninitialized bytes are two compiler-emitted four-byte
`__isa_available_default` symbols before linking. The 991 bytes sum the
`.rdata` sections as stored in their respective objects. All objects have
zero `.data` or `.tls` contribution. The unresolved symbols include 45
distinct `__imp_` names, runtime helpers such as `__chkstk`,
`__CxxFrameHandler4`, `memcpy`, and the undefined `Catalogue Data`.
These counts describe the object inputs. Linked image sections, import
resolution, TLS, static initialization and runtime contents still need
evidence for the proposed linked profile.

| Selected emitted function | Fixed RSP decrement (bytes) |
| --- | ---: |
| `supervisor_main` guard | 191,160 |
| `validate_request_recipe` | 2,328 |
| `validate_path` | 3,256 |
| `stage_scan` | 2,872 |
| `open_capture_owners` | 1,208 |
| `create_contained_target` | 424 |
| `worker_entry` | 104 |
| `publish_ready` | 88 |
| `cleanup_prefix` | 136 |

The guarded `supervisor_main` compiled to an 80-byte code section. Its only
references are `__chkstk`, `__security_cookie`, the `State` constructor
and `finish`. The last instructions store false, read the volatile byte,
and unconditionally invoke `finish` with code 10. The compiler removed the
body after the guard. The 191,160-byte frame therefore belongs specifically
to the guarded entry and cannot be used to size a future admitted entry
or total active stack. Individual function frames also exclude the return
address and any called function frames. The existing source-object sums
and these emitted frames overlap and cannot be added as independent owners.

## Source ownership and host terms

The root layout comes from the measured `sup.h` profile. The current native
source identities are recorded in the [R22 source review](artifacts/2026-11-14-clock-r22-guard-read/review.md).

| Phase | Source-owned storage and lifetime |
| --- | --- |
| State and child inputs | One 191,064-byte `State` owns two 4,256-byte worker objects with 4,096-byte buffers inside them. It also contains the 65,536-byte hash buffer, 41,260-byte request capture, 308-byte terminal data, and fixed command and environment arrays. The arrays are 58,542 and 8,312 bytes respectively. They remain part of the same root object across preparation, target setup and cleanup. |
| Pipe and log setup | One call to `open_capture_owners` visits at most two logs then two pipe creators. The 24-byte `SECURITY_ATTRIBUTES` is live during creation of both pipe pairs and NUL. At most two output buffers inside the workers are controlled capture payload. Pipe kernel backing is a separate quantity. |
| Suspended target creation | A 144-byte `JOBOBJECT_EXTENDED_LIMIT_INFORMATION`, 112-byte `STARTUPINFOEXW` and 24-byte `PROCESS_INFORMATION` can share the 280-byte selected local aggregate scope. The inherited 24-byte handle array and eight-byte job array are fields of `State`. The attribute list alone uses one independent caller-owned heap request. |
| Worker startup and cleanup | At most two workers each have three events and an owned thread reference. The worker reference never clones `State`. The source has 22 named native handle slots including worker, log, file, search and process/job slots. This counts slot capacity, including slots that may have disjoint lifetimes. The inherited handle arrays are aliases. Cleanup's job query adds a scoped 48-byte accounting structure. |
| Retained failure | The raw failed pipe pair representations and raw process output occupy 32 and 24 bytes of `State`; they are never passed as trusted handles. An unsuccessful close leaves its slot owned. A worker stays live until settlement, including its buffer and possible native request. The controller's quarantine prevents starting another child when the helper cannot establish safe retirement. |

The exact source admits one attribute allocation only if the null query for
two entries returns `FALSE`, error `ERROR_INSUFFICIENT_BUFFER`, and a
nonzero extent exactly equal to `Data.attribute_bytes`. The separate local
[host observation](artifacts/2026-11-14-clock-attribute-size/result.json)
reported 72 bytes. That establishes a local candidate for the explicit
requested allocation extent, conditional on association with the same host
API profile and future binding. It supplies no measured heap rounding or
list initialization behavior. The two source value arrays remain in
`State` until deletion. Their lifetime follows the published API contract.
[Attribute list query](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-initializeprocthreadattributelist),
[attribute value lifetime](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute)

`CreatePipe` receives zero for each pair. Microsoft documents zero as the
system default and even a nonzero size as a request rather than a maximum.
There is no numeric pipe backing upper from these arguments. For workers,
`CreateThread` receives zero stack size and no override flags. Stack
reserve and commit must therefore be read from the eventual exact PE profile
and associated with the three possible helper threads. A cancellation
request is also insufficient to prove the I/O and worker are retired.
[Pipe sizing](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe),
[thread stack sizing](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-stack-size),
[synchronous I/O cancellation](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelsynchronousio)

The source creates at most one direct target. The private job uses
`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` without an active process or memory
limit. It provides containment and a retirement check rather than a
numeric bound on temporary descendants or their backing. The documented
`CreateProcessW` contract also allows mutation of the command-line buffer
and provides the selected environment to the child. The source arrays
above qualify the owner's side only. Controlled copies in a future child
profile still need association and lifetime accounting.
[Process inputs](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)

The next join retains the exact helper PE profile, source-selected target
copies, any included backing terms, and the controller payload across its
process wait and quarantine. The shared 32 MiB controlled scratch result
cannot be inferred from the selected source and object measurements here.

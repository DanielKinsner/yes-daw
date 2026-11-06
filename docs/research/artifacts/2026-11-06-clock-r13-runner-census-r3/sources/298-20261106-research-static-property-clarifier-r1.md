# Research static property: primitive getter, not enum data

Under the existing research model, StringComparer.Ordinal and OrdinalIgnoreCase
are static property getters. Each has TypeR1, primitive dispatch/entry/return3,
and one actual caller destination W1. When used directly as a constructor
argument, that destination is the constructor parameter: total5, not two writes.
There is no instance receiver on the static getter.

The containing HashSet constructor separately has TypeR1, primitive
dispatch/entry/return3 and its result destination W1: total5. The combined direct
getter/argument and constructor group is therefore10 under these declared
boundaries. Its SDK constructor/getter bodies remain opaque. No comparer field
loop, allocation tariff or hidden binder/reflection CPU is introduced.

Actual enum/static-data access remains its existing separate form. Existing
AssertCodecBinding's JsonConvert.DefaultSettings getter and GetHash's
HashAlgorithmName.SHA256 getter provide consistency witnesses, not an excuse to
inherit all old prices. The independent critic must inventory every actual
affected caller/body sponsor across the complete frozen proposal.

Original research conventions DE33D91712372C2FE887A02FB137AA9A96C8C3BB93CB0D88BF57AA32DBEAE564
and all frozen source/censuses remain unchanged. No replacement whole or helper
price, Data, cap, native/resource fit or target execution authority follows.

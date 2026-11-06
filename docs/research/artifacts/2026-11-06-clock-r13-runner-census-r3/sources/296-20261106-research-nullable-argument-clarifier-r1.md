# Research nullable argument: one semantic conversion

Use the existing research call/conversion boundary for the new dependency
reader's `JsonSerializerSettings.MaxDepth = 24` assignment. The source literal
is Int32; the public setter parameter is Nullable<Int32>. Count one nonidentity
caller argument conversion O1, in addition to its already sponsored parameter
destination. This follows the actual Int32-to-Int64 argument coercion rule.

The selected Nullable boxing representation does not erase this distinct typed
argument formation. It supplies no extra field loop, numeric conversion inside
the setter, hidden reflection/binder/CLR body cost, stored-box size or allocation
claim. Public Boolean setters remain Boolean arguments when their signatures
are Boolean; a nullable private backing field does not change their caller type.

The original research conventions DE33D91712372C2FE887A02FB137AA9A96C8C3BB93CB0D88BF57AA32DBEAE564
and frozen R13 proposal stay unchanged. This is a narrow classification for the
whole independent critic. It accepts no replacement helper, phase or scalar
price, defines no Data, changes no cap and permits no target execution or fit.

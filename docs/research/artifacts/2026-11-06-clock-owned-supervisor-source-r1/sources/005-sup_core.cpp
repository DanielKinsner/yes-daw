#include "sup.h"

namespace yesdaw_clock_supervisor_r1 {
bool checked_add(U64 a, U64 b, U64& out) noexcept {
    if (b > UINT64_MAX - a) return false;
    out = a + b;
    return true;
}
bool charge(Meter& meter, Family family, U64 count) noexcept {
    // Meter/error leaf operations are included in the caller's source sponsor;
    // neither recursively charges itself. Tail work has its complete static bank.
    AcquireSRWLockExclusive(&meter.lock);
    bool ok = true;
    if (!meter.tail) {
        const U32 index = static_cast<U32>(family);
        if (!meter.valid || index >= static_cast<U32>(Family::Count)) {
            meter.valid = false;
            ok = false;
        } else {
            const U64 price = Data.prices[index];
            if (price == 0 || (count != 0 && price > UINT64_MAX / count)) {
                meter.valid = false;
                ok = false;
            } else {
                const U64 cost = price * count;
                if (meter.used > meter.limit || cost > meter.limit - meter.used) {
                    meter.refused = true;
                    ok = false;
                } else meter.used += cost;
            }
        }
    }
    ReleaseSRWLockExclusive(&meter.lock);
    return ok;
}
bool admit_read(Meter& meter) noexcept {
    // A ticket is linearized under the same lock as BeginTail. A ticket
    // admitted before the cut may enter/return from ReadFile afterwards.
    AcquireSRWLockExclusive(&meter.lock);
    bool ok = !meter.tail && meter.valid && !meter.refused;
    if (ok) {
        const U64 price = Data.prices[static_cast<U32>(Family::WorkerRead)];
        if (price == 0) { meter.valid = false; ok = false; }
        else if (meter.used > meter.limit || price > meter.limit - meter.used) {
            meter.refused = true; ok = false;
        } else meter.used += price;
    }
    if (!ok && !meter.tail) {
        meter.tail = true;
        meter.frozen_prefix = meter.used;
    }
    ReleaseSRWLockExclusive(&meter.lock);
    return ok;
}
void request_tail(Meter& meter) noexcept {
    AcquireSRWLockExclusive(&meter.lock);
    if (!meter.tail) { meter.tail = true; meter.frozen_prefix = meter.used; }
    ReleaseSRWLockExclusive(&meter.lock);
}
bool tail_requested(Meter& meter) noexcept {
    AcquireSRWLockShared(&meter.lock);
    const bool result = meter.tail;
    ReleaseSRWLockShared(&meter.lock);
    return result;
}
Failure failure(Site site, Reason reason, bool known, DWORD error) noexcept {
    return Failure{static_cast<U32>(site), static_cast<U32>(reason),
                   known ? error : 0u, known};
}
void fail(State& state, Site site, Reason reason, bool known, DWORD error) noexcept {
    const Failure item = failure(site, reason, known, error);
    Terminal& terminal = state.terminal;
    if (terminal.primary.site == 0) {
        terminal.primary = item;
        if (known) terminal.flags |= PrimaryNativeKnown;
    } else {
        if (terminal.secondary_count == UINT64_MAX) terminal.flags |= SecondaryCountSaturated;
        else ++terminal.secondary_count;
        if ((terminal.flags & HasSecondary) == 0) {
            terminal.first_secondary = item;
            terminal.flags |= HasSecondary;
            if (known) terminal.flags |= FirstSecondaryOwnNativeKnown;
        }
        if (known && (terminal.flags & FirstNativeSecondaryKnown) == 0) {
            terminal.first_native_secondary = item;
            terminal.flags |= FirstNativeSecondaryKnown;
        }
    }
    state.body_failed = true;
    state.stop_body = true;
    if (state.record_frozen) state.tail_failed = true;
}
void begin_tail(State& state) noexcept {
    AcquireSRWLockExclusive(&state.meter.lock);
    if (!state.meter.tail) {
        state.meter.tail = true;
        state.meter.frozen_prefix = state.meter.used;
    }
    if (state.meter.valid && state.meter.frozen_prefix <= state.meter.limit) {
            state.terminal.prefix_used = state.meter.frozen_prefix;
            state.terminal.flags |= PrefixKnown;
        } else {
            state.terminal.prefix_used = 0;
            state.terminal.flags &= ~PrefixKnown;
    }
    const bool report = !state.meter.reported;
    state.meter.reported = true;
    const bool valid = state.meter.valid;
    const bool refused = state.meter.refused;
    ReleaseSRWLockExclusive(&state.meter.lock);
    if (report && !valid) fail(state, Site::Meter, Reason::MeterOverflow);
    else if (report && refused) fail(state, Site::Meter, Reason::SourcePrefixLimit);
}
bool equal_digest(const Digest& a, const Digest& b) noexcept {
    for (U32 i = 0; i < 32; ++i) if (a.bytes[i] != b.bytes[i]) return false;
    return true;
}
bool equal_wide(const wchar_t* a, U32 an, const wchar_t* b, U32 bn, bool fold) noexcept {
    if (an != bn) return false;
    if (!fold) {
        for (U32 i = 0; i < an; ++i) if (a[i] != b[i]) return false;
        return true;
    }
    if (an > path_units || bn > path_units) return false;
    return CompareStringOrdinal(a, static_cast<int>(an), b,
                                static_cast<int>(bn), TRUE) == CSTR_EQUAL;
}

namespace {
constexpr U32 sha_constants[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
U32 rotate(U32 value, U32 count) noexcept {
    return (value >> count) | (value << (32u - count));
}
bool compress(State& state, Sha256& sha, const U8* block) noexcept {
    if (!charge(state.meter, Family::HashBlock) ||
        !charge(state.meter, Family::HashRound, 64)) return false;
    U32 words[64];
    for (U32 i = 0; i < 16; ++i) {
        const U32 at = 4u * i;
        words[i] = (static_cast<U32>(block[at]) << 24) |
                   (static_cast<U32>(block[at + 1]) << 16) |
                   (static_cast<U32>(block[at + 2]) << 8) |
                   static_cast<U32>(block[at + 3]);
    }
    for (U32 i = 16; i < 64; ++i) {
        const U32 x = words[i - 15], y = words[i - 2];
        const U32 small0 = rotate(x, 7) ^ rotate(x, 18) ^ (x >> 3);
        const U32 small1 = rotate(y, 17) ^ rotate(y, 19) ^ (y >> 10);
        words[i] = words[i - 16] + small0 + words[i - 7] + small1;
    }
    U32 a=sha.h[0], b=sha.h[1], c=sha.h[2], d=sha.h[3];
    U32 e=sha.h[4], f=sha.h[5], g=sha.h[6], h=sha.h[7];
    for (U32 i = 0; i < 64; ++i) {
        const U32 large1 = rotate(e,6) ^ rotate(e,11) ^ rotate(e,25);
        const U32 choose = (e & f) ^ (~e & g);
        const U32 one = h + large1 + choose + sha_constants[i] + words[i];
        const U32 large0 = rotate(a,2) ^ rotate(a,13) ^ rotate(a,22);
        const U32 majority = (a & b) ^ (a & c) ^ (b & c);
        const U32 two = large0 + majority;
        h=g; g=f; f=e; e=d+one; d=c; c=b; b=a; a=one+two;
    }
    sha.h[0]+=a; sha.h[1]+=b; sha.h[2]+=c; sha.h[3]+=d;
    sha.h[4]+=e; sha.h[5]+=f; sha.h[6]+=g; sha.h[7]+=h;
    return true;
}
}
bool sha_init(State& state, Sha256& sha) noexcept {
    if (!charge(state.meter, Family::HashChunk)) return false;
    sha.h[0]=0x6a09e667; sha.h[1]=0xbb67ae85;
    sha.h[2]=0x3c6ef372; sha.h[3]=0xa54ff53a;
    sha.h[4]=0x510e527f; sha.h[5]=0x9b05688c;
    sha.h[6]=0x1f83d9ab; sha.h[7]=0x5be0cd19;
    sha.pending_count=0; sha.bytes=0;
    return true;
}
bool sha_update(State& state, Sha256& sha, const U8* data, U32 bytes) noexcept {
    if (!charge(state.meter, Family::HashChunk) ||
        !charge(state.meter, Family::HashByte, bytes)) return false;
    if (bytes > UINT64_MAX - sha.bytes) {
        fail(state, Site::Meter, Reason::MeterOverflow);
        return false;
    }
    sha.bytes += bytes;
    for (U32 i = 0; i < bytes; ++i) {
        sha.pending[sha.pending_count++] = data[i];
        if (sha.pending_count == 64) {
            if (!compress(state,sha,sha.pending)) return false;
            sha.pending_count = 0;
        }
    }
    return true;
}
bool sha_finish(State& state, Sha256& sha, Digest& digest) noexcept {
    if (!charge(state.meter, Family::HashChunk)) return false;
    if (sha.bytes > UINT64_MAX / 8u) {
        fail(state,Site::Meter,Reason::MeterOverflow);
        return false;
    }
    const U64 bits = sha.bytes * 8u;
    U8 padding[128];
    U32 count = sha.pending_count < 56 ? 56u-sha.pending_count : 120u-sha.pending_count;
    padding[0]=0x80;
    for (U32 i=1;i<count;++i) padding[i]=0;
    for (U32 i=0;i<8;++i) padding[count+i]=static_cast<U8>(bits >> (56u-8u*i));
    if (!sha_update(state,sha,padding,count+8u) || sha.pending_count!=0) return false;
    for (U32 i=0;i<8;++i) {
        digest.bytes[4*i]=static_cast<U8>(sha.h[i]>>24);
        digest.bytes[4*i+1]=static_cast<U8>(sha.h[i]>>16);
        digest.bytes[4*i+2]=static_cast<U8>(sha.h[i]>>8);
        digest.bytes[4*i+3]=static_cast<U8>(sha.h[i]);
    }
    return true;
}
bool hash_run_identity(State& state, Digest& digest) noexcept {
    if (!charge(state.meter,Family::Identity)) return false;
    static constexpr U8 domain[] = "YESDAW-CLOCK-SUPERVISOR-RUN-V1";
    Sha256 sha;
    if (!sha_init(state,sha) || !sha_update(state,sha,domain,static_cast<U32>(sizeof(domain))) ||
        !sha_update(state,sha,state.request.manifest_sha.bytes,32)) return false;
    U8 length[4];
    for (U32 i=0;i<4;++i) length[i]=static_cast<U8>(state.request.working.length>>(8u*i));
    if (!sha_update(state,sha,length,4)) return false;
    for (U32 i=0;i<state.request.working.length;++i) {
        const U32 cell=static_cast<U32>(state.request.working.cells[i]);
        const U8 encoded[2]={static_cast<U8>(cell),static_cast<U8>(cell>>8)};
        if (!sha_update(state,sha,encoded,2)) return false;
    }
    return sha_finish(state,sha,digest);
}
}

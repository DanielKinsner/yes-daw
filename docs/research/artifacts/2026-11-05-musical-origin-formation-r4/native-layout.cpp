#include "formation.h"
#include <cstdio>

namespace f = yesdaw::app::musical_origin_source_r4;
constexpr auto rawBytes = static_cast<f::U64> (sizeof (f::Raw));
constexpr f::U64 d = std::is_same_v<f::U64, std::size_t> ? 0 : 1;
constexpr f::U64 root = 9 * rawBytes + 15471 + 3 * d;
static_assert (rawBytes > 0 && rawBytes <= 29824442);
static_assert (root <= 268435452 && root < f::workCap);
static_assert (sizeof (f::MusicalPreparationRequestState) <= f::byteCap);
static_assert (alignof (f::MusicalPreparationRequestState) <= alignof (std::max_align_t));
static_assert (std::numeric_limits<std::size_t>::max() >= f::byteCap);

int main()
{
    std::printf ("{\"raw_bytes\":%zu,\"state_bytes\":%zu,\"state_alignment\":%zu,"
                 "\"metadata_bytes\":%zu,\"parents_bytes\":%zu,\"map_row_bytes\":%zu,"
                 "\"application_bytes\":%zu,\"control_owner_bytes\":%zu,\"slot_bytes\":%zu,"
                 "\"size_t_bytes\":%zu,\"conversion_unit\":%llu,\"root_reservation\":%llu}\n",
                 sizeof (f::Raw), sizeof (f::MusicalPreparationRequestState), alignof (f::MusicalPreparationRequestState),
                 sizeof (f::CapturedMetadata), sizeof (f::Parents), sizeof (yesdaw::engine::TempoChange),
                 sizeof (f::ApplicationRequests), sizeof (f::ControlRequestOwner), sizeof (f::OwnerSlot),
                 sizeof (std::size_t), static_cast<unsigned long long> (d), static_cast<unsigned long long> (root));
    return 0;
}

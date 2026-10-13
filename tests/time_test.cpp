// RtlTimeToTimeFields / RtlTimeFieldsToTime against known dates.

#include "test_support.h"

namespace {

struct TimeFields {
    int16_t year, month, day, hour, minute, second, millisecond, weekday;
};

using ToFields = void(XB_STDCALL*)(const int64_t*, TimeFields*);
using ToTime = uint32_t(XB_STDCALL*)(const TimeFields*, int64_t*);

}  // namespace

int main() {
    const auto to_fields = test::find_export<ToFields>("RtlTimeToTimeFields");
    const auto to_time = test::find_export<ToTime>("RtlTimeFieldsToTime");

    // 1601-01-01 00:00:00.000, a Monday.
    int64_t t = 0;
    TimeFields f{};
    to_fields(&t, &f);
    CHECK(f.year == 1601 && f.month == 1 && f.day == 1 && f.hour == 0 && f.weekday == 1);

    // 1970-01-01 (Thursday) = 116444736000000000.
    t = 116444736000000000LL;
    to_fields(&t, &f);
    CHECK(f.year == 1970 && f.month == 1 && f.day == 1 && f.weekday == 4, "%d-%d-%d wd %d", f.year, f.month, f.day,
          f.weekday);

    // 2004-02-29 13:45:30.250 (a Sunday; leap day) round trip.
    const TimeFields leap{2004, 2, 29, 13, 45, 30, 250, 0};
    int64_t leap_time = 0;
    CHECK(to_time(&leap, &leap_time) == 1);
    to_fields(&leap_time, &f);
    CHECK(f.year == 2004 && f.month == 2 && f.day == 29 && f.hour == 13 && f.minute == 45 && f.second == 30 &&
              f.millisecond == 250 && f.weekday == 0,
          "%d-%d-%d %d:%d:%d.%d wd %d", f.year, f.month, f.day, f.hour, f.minute, f.second, f.millisecond, f.weekday);
    // 1970-based check of the same instant: 1078062330 s since the Unix epoch.
    CHECK(leap_time == 116444736000000000LL + 1078062330250LL * 10000, "%lld", static_cast<long long>(leap_time));

    // Invalid fields are rejected.
    const TimeFields feb30{2005, 2, 30, 0, 0, 0, 0, 0};
    const TimeFields month13{2005, 13, 1, 0, 0, 0, 0, 0};
    int64_t untouched = 42;
    CHECK(to_time(&feb30, &untouched) == 0 && untouched == 42);
    CHECK(to_time(&month13, &untouched) == 0 && untouched == 42);

    if (test::g_failures) return 1;
    std::printf("time_test passed\n");
    return 0;
}

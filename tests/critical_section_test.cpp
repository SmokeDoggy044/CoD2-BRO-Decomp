// RtlEnter/LeaveCriticalSection under contention, with recursion, on both an
// initialized and a statically defined (never initialized) critical section.

#include <thread>
#include <vector>

#include "test_support.h"
#include "types.h"

using xbox::XbCriticalSection;
using CsFn = void(XB_STDCALL*)(XbCriticalSection*);

namespace {

void hammer(CsFn enter, CsFn leave, XbCriticalSection& cs, int threads, int iterations, long& counter) {
    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t)
        workers.emplace_back([&] {
            test::attach_thread();
            for (int i = 0; i < iterations; ++i) {
                enter(&cs);
                enter(&cs);  // recursive
                const long value = counter;
                counter = value + 1;
                leave(&cs);
                leave(&cs);
            }
        });
    for (auto& worker : workers) worker.join();
}

}  // namespace

int main() {
    test::init_kernel();
    const auto init = test::find_export<CsFn>("RtlInitializeCriticalSection");
    const auto enter = test::find_export<CsFn>("RtlEnterCriticalSection");
    const auto leave = test::find_export<CsFn>("RtlLeaveCriticalSection");

    XbCriticalSection cs{};
    init(&cs);
    CHECK(cs.lock_count == -1 && cs.synchronization.type == 1);
    long counter = 0;
    hammer(enter, leave, cs, 8, 100000, counter);
    CHECK(counter == 800000, "counter %ld", counter);
    CHECK(cs.lock_count == -1 && cs.recursion_count == 0 && cs.owning_thread == 0, "lock %d recursion %d owner %x",
          cs.lock_count, cs.recursion_count, cs.owning_thread);

    // As XAPI defines them in .data: event type 1, size 4, LockCount -1, no init call.
    XbCriticalSection static_cs{};
    static_cs.synchronization.type = 1;
    static_cs.synchronization.size = 4;
    static_cs.lock_count = -1;
    counter = 0;
    hammer(enter, leave, static_cs, 4, 50000, counter);
    CHECK(counter == 200000, "counter %ld", counter);
    CHECK(static_cs.lock_count == -1);

    // The owner shows up in OwningThread while held.
    enter(&cs);
    CHECK(cs.owning_thread == test::t_kthread && cs.recursion_count == 1);
    leave(&cs);
    return test::finish("critical_section_test");
}

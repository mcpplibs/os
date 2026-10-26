module mcpplibs.os.interrupt;

import std;
import openkal.types;
import openkal.process;
import openkal.task;

namespace mcpplibs::os::interrupt {

namespace {
kal_u32 load(const kal_u32* w) {
    return std::atomic_ref<kal_u32>(*const_cast<kal_u32*>(w)).load(std::memory_order_acquire);
}

const kal_u32* word() {
    static const kal_u32* w = kal_process_stop_requested();
    return w;
}
}  // namespace

bool available() { return word() != nullptr; }

bool requested() {
    const kal_u32* w = word();
    return w != nullptr && load(w) != 0;
}

bool wait(std::optional<std::chrono::milliseconds> timeout) {
    const kal_u32* w = word();
    if (w == nullptr) {
        if (timeout) std::this_thread::sleep_for(*timeout);
        return false;
    }
    const auto until = timeout ? std::chrono::steady_clock::now() + *timeout
                               : std::chrono::steady_clock::time_point::max();
    while (load(w) == 0) {
        kal_u64 ns = 0;   // zero waits without a bound
        if (timeout) {
            const auto left = until - std::chrono::steady_clock::now();
            if (left <= std::chrono::steady_clock::duration::zero()) return false;
            ns = static_cast<kal_u64>(std::max<std::int64_t>(
                1, std::chrono::duration_cast<std::chrono::nanoseconds>(left).count()));
        }
        kal_task_wait(w, 0, ns);
    }
    return true;
}

void end_as_requested() {
    if (!requested()) return;
    kal_process_end_as_requested();
}

void end_as_output_closed() { sys::end_as_output_closed(); }

}  // namespace mcpplibs::os::interrupt

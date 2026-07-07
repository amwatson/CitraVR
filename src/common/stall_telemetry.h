// Copyright Citra Emulator Project / CitraVR
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace Common {

/**
 * Process-wide, lock-free counters used to diagnose frame stalls. Written from
 * the emu/GPU thread (core frame timing, draw-time shader compiles) and the VR
 * frame thread (compositor deadline misses), read from the stats panel and the
 * user stall-marker logger.
 *
 * All counters are cumulative since process start; readers wanting
 * per-interval values should diff successive reads. "Worst" values are
 * all-time maxima.
 */
struct StallTelemetry {
    // Draw-time shader compiles on the emu/GPU thread (GLES compile+link on a
    // shader-cache miss, including the disk-cache write). The dominant cause
    // of one-off stalls in Citra-family emulators.
    std::atomic<std::uint32_t> shader_compile_count{0};
    std::atomic<std::uint64_t> shader_compile_time_us{0};
    // Steady-clock timestamp (us since epoch of steady_clock) of the most
    // recent draw-time compile; 0 if none has happened yet.
    std::atomic<std::int64_t> last_shader_compile_steady_us{0};

    // Core emulation frames that blew their vblank budget (see perf_stats.cpp
    // for thresholds).
    std::atomic<std::uint32_t> emu_slow_frame_count{0};
    std::atomic<std::uint32_t> emu_stall_frame_count{0};
    std::atomic<std::uint64_t> emu_worst_frame_time_us{0};

    // VR frame loop (see vr_main.cpp). A "missed" frame is a gap between
    // successive xrEndFrame completions exceeding 1.5x the display period.
    std::atomic<std::uint32_t> vr_missed_frame_count{0};
    std::atomic<std::uint64_t> vr_worst_frame_gap_us{0};
    std::atomic<std::uint64_t> vr_worst_wait_frame_us{0};

    // User-initiated stall markers (thumbstick click in VR).
    std::atomic<std::uint32_t> user_marker_count{0};

    static StallTelemetry& Get() {
        static StallTelemetry instance;
        return instance;
    }
};

inline void UpdateMax(std::atomic<std::uint64_t>& target, std::uint64_t value) {
    std::uint64_t current = target.load(std::memory_order_relaxed);
    while (current < value &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

inline std::int64_t SteadyNowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace Common

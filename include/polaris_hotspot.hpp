#pragma once
#include <cstdint>
#include <stdexcept>

namespace polaris {
constexpr std::uint32_t smu_index = 0x210; // BAR5 byte offsets, not DWORD indices
constexpr std::uint32_t smu_data = 0x214;
constexpr std::uint32_t thermal_status = 0xC0300014;
constexpr unsigned decode_hotspot(std::uint32_t raw) { return raw & 0x1FFu; }

// Caller owns exclusive access to the shared selector for the whole call.
// read/write accept BAR5 byte offsets. Transport must report failures by throwing.
// This reads telemetry; it does not request a new sensor conversion.
template<class Read32, class Write32>
std::uint32_t read_thermal_word(Read32 read, Write32 write) {
    const auto saved = read(smu_index);
    if (saved == 0xFFFFFFFFu) throw std::runtime_error("Invalid SMU selector");
    try {
        write(smu_index, thermal_status);
        if (read(smu_index) != thermal_status)
            throw std::runtime_error("SMU selection failed");
        const auto raw = read(smu_data);
        if (read(smu_index) != thermal_status)
            throw std::runtime_error("SMU selector changed");
        write(smu_index, saved);
        if (read(smu_index) != saved)
            throw std::runtime_error("SMU restoration failed");
        if (raw == 0 || raw == 0xFFFFFFFFu)
            throw std::runtime_error("Invalid thermal word");
        return raw;
    } catch (...) {
        // Best-effort restoration on transport errors; never turn a failure into a sample.
        write(smu_index, saved);
        throw;
    }
}
}

#pragma once
#include <cstdint>

namespace polaris {
// Polaris 10/11/12 PCI IDs from Linux v6.12 amdgpu_drv.c.
// Eligibility does not imply that a board was tested on our bench.
constexpr bool accepts_device(std::uint32_t vendor, std::uint32_t device) {
    if (vendor != 0x1002) return false;
    switch (device) {
    case 0x67C0: case 0x67C1: case 0x67C2: case 0x67C4: case 0x67C7:
    case 0x67D0: case 0x67DF: case 0x67C8: case 0x67C9: case 0x67CA:
    case 0x67CC: case 0x67CF: case 0x6FDF:
    case 0x67E0: case 0x67E3: case 0x67E8: case 0x67EB: case 0x67EF:
    case 0x67FF: case 0x67E1: case 0x67E7: case 0x67E9:
    case 0x6980: case 0x6981: case 0x6985: case 0x6986: case 0x6987:
    case 0x6995: case 0x6997: case 0x699F:
        return true;
    default: return false;
    }
}
constexpr bool tested_profile(std::uint32_t device, std::uint32_t subsystem, std::uint32_t revision) {
    return (device == 0x699F && subsystem == 0x0B041002 && revision == 0xC7) ||
           (device == 0x6FDF && (subsystem == 0xE5801682 || subsystem == 0x0B311002) && revision == 0xEF);
}
}

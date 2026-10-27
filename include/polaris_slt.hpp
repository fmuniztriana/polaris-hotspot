#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>

namespace polaris {
// LOT (base 37) + wafer + X + Y. Numeric fields use at least two decimal digits.
inline std::string sid_to_slt(std::uint64_t primary,std::uint64_t extended) {
    if(!primary || primary >= (1ULL<<50) || extended >= (1ULL<<43))
        throw std::invalid_argument("Invalid Polaris SID width or value");
    constexpr char alphabet[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uint32_t lot=static_cast<std::uint32_t>(primary);
    std::string result;
    do {result.insert(result.begin(),alphabet[lot%37]);lot/=37;} while(lot);
    auto append=[&](unsigned value){auto text=std::to_string(value);if(text.size()<2)result+='0';result+=text;};
    append(static_cast<unsigned>((primary>>32)&31));
    append(static_cast<unsigned>((primary>>37)&255));
    append(static_cast<unsigned>(extended&255));
    return result;
}
}

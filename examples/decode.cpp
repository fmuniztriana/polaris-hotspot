#include "polaris_hotspot.hpp"
#include <iostream>
#include <string>
#include <limits>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Usage: decode 0x00007242\n"; return 1; }
    try {
        const std::string text = argv[1];
        std::size_t used = 0;
        const auto value = std::stoull(text, &used, 0);
        if (used != text.size() || text.empty() || text[0] == '-' ||
            value > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("Expected a 32-bit unsigned word");
        if (value == 0 || value == 0xFFFFFFFFu)
            throw std::runtime_error("Invalid thermal word");
        std::cout << polaris::decode_hotspot(static_cast<std::uint32_t>(value)) << " C\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

#pragma once
#include <cstdint>

namespace worlddiff::dissolve {
enum class Style : std::uint8_t { Silver, Amber, Ink };
inline constexpr Style DefaultStyle = Style::Amber;
inline constexpr const char* StyleNames[] = {"银紫流光", "琥珀裂晶", "墨染洇花"};
}

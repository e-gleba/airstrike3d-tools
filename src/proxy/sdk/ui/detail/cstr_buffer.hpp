#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <string_view>

namespace sdk::ui::detail
{

/// Fixed-size, null-terminated view for ImGui C APIs. Truncates silently.
template <std::size_t Capacity = 512>
class cstr_buffer final
{
public:
    explicit cstr_buffer(std::string_view text) noexcept
    {
        const auto length = std::min(text.size(), Capacity - 1);
        std::copy_n(text.data(), length, buffer_.begin());
        buffer_[length] = '\0';
    }

    [[nodiscard]] const char* c_str() const noexcept { return buffer_.data(); }

private:
    std::array<char, Capacity> buffer_{};
};

} // namespace sdk::ui::detail

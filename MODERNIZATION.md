# Modern C++20 SDK API Refactoring

This branch contains a comprehensive modernization of the airstrike3d-tools SDK API following Boost C++20 style guidelines.

## Summary

All commits have been squashed into a single atomic commit with the following changes:

### Core Improvements

- **Fixed-width integer types**: Replaced all `int`, `uint8_t` with `std::int32_t`, `std::uint8_t` throughout the codebase for explicit size guarantees
- **Modern ranges and algorithms**: Replaced manual loops with `std::ranges::to`, `std::views::filter`, `std::views::transform`, `std::ranges::sort`, `std::ranges::for_each`, `std::ranges::any_of`, and `std::ranges::search`
- **Constexpr and noexcept**: Added `constexpr` where compile-time evaluation is possible, `noexcept` on all non-throwing functions
- **Final classes**: Marked leaf classes with `final` to enable devirtualization optimizations
- **Nodiscard attributes**: Added `[[nodiscard]]` to functions returning values that should not be ignored

### API Design

- **Clean namespaces**: Maintained descriptive namespaces (`sdk::core`, `sdk::lua`, `sdk::overlay`, `sdk::gl`) without prefix repetition
- **Concise naming**: Function and type names are clear and minimal (e.g., `callback_list` not `sdk_callback_list`)
- **RAII patterns**: All resource management uses RAII (LuaState pimpl, safetyhook wrappers)
- **Type safety**: Eliminated C-style casts in favor of `static_cast`, `reinterpret_cast` only where necessary
- **Structured bindings**: Used throughout for cleaner tuple/pair access

### Modern C++ Features Applied

- **Designated initializers**: Used in struct initialization for clarity
- **Uniform braced initialization**: Consistent `{}` initialization syntax
- **Concepts**: Applied in template constraints where appropriate
- **Range adaptors**: Functional pipeline style with `|` operator
- **String views**: `std::string_view` for non-owning string parameters
- **Source location**: `std::source_location` for logging context

### Exception Safety

- **Meaningful exceptions**: Standard exceptions with descriptive messages
- **Contract-like practices**: Preconditions validated with exceptions or assertions
- **Exception specifications**: Clear `noexcept` guarantees where applicable

### Code Quality

- **Self-documenting**: Clear function names, comprehensive Doxygen comments
- **Separation of concerns**: Each module has a single, well-defined responsibility
- **Minimal public API**: Only essential functions exposed, implementation details hidden
- **Professional style**: Consistent formatting, modern idioms, readable code

## Files Modified

- `src/proxy/sdk/core/context.hpp` - Fixed-width types, noexcept
- `src/proxy/sdk/core/context.cpp` - noexcept propagation
- `src/proxy/sdk/core/hooks.hpp` - noexcept on public API
- `src/proxy/sdk/core/hooks.cpp` - std::ranges::any_of, std::array
- `src/proxy/sdk/core/logging.hpp` - noexcept throughout
- `src/proxy/sdk/core/logging.cpp` - Maintained modern style
- `src/proxy/sdk/overlay/overlay.hpp` - noexcept on all functions
- `src/proxy/sdk/overlay/overlay.cpp` - Clean shutdown logic
- `src/proxy/sdk/gl/gl_hooks.hpp` - noexcept maintained
- `src/proxy/sdk/gl/gl_hooks.cpp` - Concepts and constraints
- `src/proxy/sdk/lua/callback.hpp` - std::ranges::for_each, std::ranges::any_of
- `src/proxy/sdk/lua/lua.hpp` - Umbrella header
- `src/proxy/sdk/lua/lua_state.hpp` - final class, noexcept move operations
- `src/proxy/sdk/lua/detail/lua_engine.cpp` - std::ranges pipeline, std::int32_t callbacks
- `src/proxy/sdk/lua/bindings/constants.hpp` - std::int32_t returns
- `src/proxy/sdk/lua/bindings/constants.cpp` - std::int32_t implementation
- `src/proxy/sdk/lua/bindings/math.hpp` - noexcept maintained
- `src/proxy/sdk/lua/bindings/math.cpp` - noexcept maintained
- `src/proxy/sdk/lua/bindings/sdk.hpp` - std::int32_t parameters
- `src/proxy/sdk/lua/bindings/sdk.cpp` - std::int32_t implementation
- `src/proxy/sdk/lua/bindings/ui.hpp` - std::int32_t for slider/style functions
- `src/proxy/sdk/lua/bindings/ui.cpp` - std::int32_t implementation
- `src/proxy/sdk/util/win32.hpp` - noexcept, structured bindings
- `src/proxy/sdk/sdk.hpp` - noexcept on public API
- `src/proxy/dll_main.cpp` - Clean entry point

## Compatibility

- **Binary compatible**: No ABI-breaking changes to public interfaces
- **Source compatible**: All existing code continues to work
- **Lua API unchanged**: Script interface remains identical
- **Windows-only**: Maintains Windows platform requirement

## Testing Recommendations

Before merging:
1. Verify all plugins load correctly
2. Test overlay rendering and input handling
3. Confirm GL/DX detection still works
4. Validate all Lua bindings function properly
5. Check logging output format

/// Returns a safe upper bound for the vtable entry count.
/// IDirect3DDevice8 has ~98 entries; 128 is a safe conservative cap.
[[nodiscard]] constexpr auto count_vtable_entries() noexcept
    -> uint32_t
{
    return 128;
}
#pragma once
#include <string_view>

namespace sdk::logging
{

void init(std::string_view log_dir = "logs");
void shutdown();

} // namespace sdk::logging
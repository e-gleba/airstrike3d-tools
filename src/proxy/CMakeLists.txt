target_link_libraries(
    bass_proxy_sdk
    PUBLIC warnings
           imgui_opengl3
           dwmapi
           glm
           glu32
           opengl32
           safetyhook
           spdlog::spdlog
           sol2::sol2
           lua)

include(FetchContent)

set(cpm_version "0.42.3")

fetchcontent_declare(
    get_cpm
    URL "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${cpm_version}/CPM.cmake"
    DOWNLOAD_NO_EXTRACT TRUE)

fetchcontent_makeavailable(get_cpm)

include("${get_cpm_SOURCE_DIR}/CPM.cmake")

# Enable local package reuse (vcpkg, system, etc.)
# Ref: https://github.com/cpm-cmake/CPM.cmake#find_package-integration
set(CPM_USE_LOCAL_PACKAGES ON)

# Write a dummy .clang-tidy into the CPM cache so dependency source files
# do not get linted by the host's clang-tidy (common with v0.42.x).
if(CPM_SOURCE_CACHE)
    file(WRITE "${CPM_SOURCE_CACHE}/.clang-tidy" "Checks: '-*'\n")
endif()

set(cpm_deps_dir "${CMAKE_CURRENT_LIST_DIR}/cpm")

list(APPEND CMAKE_PREFIX_PATH "${cpm_deps_dir}")
if(CMAKE_CROSSCOMPILING)
    list(APPEND CMAKE_FIND_ROOT_PATH "${cpm_deps_dir}")
endif()

find_package(freetype CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(safetyhook CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(lua_lang CONFIG REQUIRED)

include(FetchContent)

set(cpm_version "0.42.1")
set(cpm_expected_hash
    "f3a6dcc6a04ce9e7f51a127307fa4f699fb2bade357a8eb4c5b45df76e1dc6a5")

fetchcontent_declare(
    get_cpm
    URL "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${cpm_version}/CPM.cmake"
    URL_HASH SHA256=${cpm_expected_hash}
    DOWNLOAD_NO_EXTRACT TRUE)

fetchcontent_makeavailable(get_cpm)

include("${get_cpm_SOURCE_DIR}/CPM.cmake")

set(CPM_USE_LOCAL_PACKAGES ON)

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

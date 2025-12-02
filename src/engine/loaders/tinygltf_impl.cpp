/// @file tinygltf_impl.cpp
/// @brief TinyGLTF library implementation
/// This file exists to contain the TinyGLTF implementation in a single
/// compilation unit. This also provides STB_IMAGE implementation used by
/// texture.cpp.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

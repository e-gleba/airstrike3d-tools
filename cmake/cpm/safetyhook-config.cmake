CPMAddPackage(
  NAME safetyhook
  GITHUB_REPOSITORY cursey/safetyhook
  GIT_TAG v0.6.9
  OPTIONS
    "SAFETYHOOK_FETCH_ZYDIS ON"
    EXCLUDE_FROM_ALL
    YES
)
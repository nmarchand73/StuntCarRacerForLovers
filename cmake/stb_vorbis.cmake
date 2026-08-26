include(FetchContent)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

if(NOT EXISTS "${stb_SOURCE_DIR}/stb_vorbis.c")
  message(FATAL_ERROR "stb_vorbis.c not found in fetched stb repository.")
endif()

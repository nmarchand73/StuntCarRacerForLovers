include(FetchContent)

set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_LITE OFF CACHE BOOL "" FORCE)
set(LIBXMP_DISABLE_DEPACKERS ON CACHE BOOL "" FORCE)
set(LIBXMP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  libxmp
  GIT_REPOSITORY https://github.com/libxmp/libxmp.git
  GIT_TAG libxmp-4.6.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libxmp)

if(TARGET xmp_static)
  set_property(TARGET xmp_static PROPERTY FOLDER "thirdparty")
elseif(TARGET libxmp::xmp_static)
  set_property(TARGET libxmp::xmp_static PROPERTY FOLDER "thirdparty")
endif()

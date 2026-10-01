set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
if (MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif ()

include(FetchContent)
FetchContent_Declare(
        googletest GIT_REPOSITORY
        "https://github.com/google/googletest.git" GIT_TAG
        "v1.17.0" GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_subdirectory(unit)
add_subdirectory(integration)

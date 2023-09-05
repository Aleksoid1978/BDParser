include(FetchContent)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
    GIT_TAG        "10.1.1"
)

FetchContent_MakeAvailable(fmt)

if(MSVC)
    if(STATIC_MSVC_CRT)
        target_compile_options(fmt PRIVATE
            "$<$<CONFIG:Debug>:/MTd>"
            "$<$<CONFIG:Release>:/MT>"
        )
    endif()
endif()

cmake_minimum_required (VERSION 3.13) # req by add_link_options()

project (ctguard VERSION 0.1)


set (Cereal_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")


option (ENABLE_DOC "Enable building documentation. [default: ON]"                       ON)
option (ENABLE_OPTIMIZATIONS "Enable optimizations. [default: ON]"                      ON)
option (ENABLE_HARDENING "Enable hardening build flags. [default: ON]"                  ON)
option (FAIL_ON_WARNINGS "Fail on compiler warnings (e.g. pass -Werror) [default: ON]"  ON)
option (ENABLE_LTO "Enable Link Time Optimization. [default: OFF]"                      OFF)
option (FORCE_COLORED_OUTPUT "Force colored output (useful with Ninja). [default: ON]"  ON)
option (ENABLE_SANITIZERS "Enable compiler sanitizers. [default: OFF]"                  OFF)
option (RUN_CLANG_TIDY "Run the clang-tidy static analyzer. [default: OFF]"             OFF)


set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED TRUE)
set (CMAKE_CXX_EXTENSIONS FALSE)


find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    # Support Unix Makefiles and Ninja
    set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    message (STATUS "Using ccache: ${CCACHE_PROGRAM}")
else ()
    message (STATUS "Not found ccache")
endif ()


if (ENABLE_OPTIMIZATIONS)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options (-O2 -ftree-vectorize -ftree-slp-vectorize)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options (-O2)
    else ()
        message (FATAL_ERROR "Do not know how to set optimization flags for compiler ${CMAKE_CXX_COMPILER_ID}!")
    endif ()
endif ()


if (ENABLE_HARDENING)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_definitions (_FORTIFY_SOURCE=2)
        add_compile_options (-fstack-protector-strong)
        add_compile_options (-fPIE)
        add_link_options (-pie)
        add_link_options (LINKER:-z,relro)
        add_link_options (LINKER:-z,now)
    else ()
        message (FATAL_ERROR "Do not know how to set hardening build flags for compiler ${CMAKE_CXX_COMPILER_ID}!")
    endif ()
endif ()


if (ENABLE_SANITIZERS)
    if (ENABLE_OPTIMIZATIONS)
        message (FATAL_ERROR "Optimization must be turned off for sanitizers!")
    endif ()

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options (-g)
        add_compile_options (-O1)

        add_compile_options (-fsanitize=address)
        add_compile_options (-fsanitize-address-use-after-scope)
        add_compile_options (-fno-omit-frame-pointer)
        add_compile_options (-fsanitize=undefined)
        add_compile_options (-fsanitize=nullability)
        add_compile_options (-fsanitize=implicit-conversion)
        add_compile_options (-fsanitize=integer)
        add_compile_options (-fsanitize=float-divide-by-zero)
        add_compile_options (-fsanitize=local-bounds)

        add_link_options (-fsanitize=address)
        add_link_options (-fsanitize-address-use-after-scope)
        add_link_options (-fno-omit-frame-pointer)
        add_link_options (-fsanitize=undefined)
        add_link_options (-fsanitize=nullability)
        add_link_options (-fsanitize=implicit-conversion)
        add_link_options (-fsanitize=integer)
        add_link_options (-fsanitize=float-divide-by-zero)
        add_link_options (-fsanitize=local-bounds)
    else ()
        message (FATAL_ERROR "Do not know how to enable sanitizers for compiler ${CMAKE_CXX_COMPILER_ID}!")
    endif ()
endif ()


if (ENABLE_LTO)
    include (CheckIPOSupported)
    check_ipo_supported ()
    set (CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    message (STATUS "Enabled IPO/LTO")
endif ()


if (FAIL_ON_WARNINGS)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list (APPEND CXX_WARN_FLAGS -Werror)
        list (APPEND C_WARN_FLAGS -Werror)
    else ()
        message (FATAL_ERROR "Do not know how to treat warnings as errors for compiler ${CMAKE_CXX_COMPILER_ID}!")
    endif ()
endif ()


if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list (APPEND CXX_WARN_FLAGS -Wall)
    list (APPEND CXX_WARN_FLAGS -Wextra)
    list (APPEND CXX_WARN_FLAGS -Wpedantic)
    list (APPEND CXX_WARN_FLAGS -Wdouble-promotion)
    list (APPEND CXX_WARN_FLAGS -Wformat=2)
    list (APPEND CXX_WARN_FLAGS -Wnull-dereference)
    list (APPEND CXX_WARN_FLAGS -Wmissing-include-dirs)
    list (APPEND CXX_WARN_FLAGS -Wunknown-pragmas)
    list (APPEND CXX_WARN_FLAGS -Wsuggest-attribute=noreturn)
    list (APPEND CXX_WARN_FLAGS -Wsuggest-attribute=format)
    list (APPEND CXX_WARN_FLAGS -Wsuggest-final-types)
    list (APPEND CXX_WARN_FLAGS -Wsuggest-final-methods)
    list (APPEND CXX_WARN_FLAGS -Wsuggest-override)
    list (APPEND CXX_WARN_FLAGS -Walloc-zero)
    list (APPEND CXX_WARN_FLAGS -Walloca)
    list (APPEND CXX_WARN_FLAGS -Warray-bounds=2)
    list (APPEND CXX_WARN_FLAGS -Wduplicated-branches)
    list (APPEND CXX_WARN_FLAGS -Wduplicated-cond)
    list (APPEND CXX_WARN_FLAGS -Wfloat-equal)
    list (APPEND CXX_WARN_FLAGS -Wshadow)
    list (APPEND CXX_WARN_FLAGS -Wpointer-arith)
    list (APPEND CXX_WARN_FLAGS -Wundef)
    list (APPEND CXX_WARN_FLAGS -Wunused-macros)
    list (APPEND CXX_WARN_FLAGS -Wcast-qual)
    list (APPEND CXX_WARN_FLAGS -Wconversion)
    list (APPEND CXX_WARN_FLAGS -Wsign-conversion)
    list (APPEND CXX_WARN_FLAGS -Wzero-as-null-pointer-constant)
    list (APPEND CXX_WARN_FLAGS -Wuseless-cast)
    list (APPEND CXX_WARN_FLAGS -Wlogical-op)
    list (APPEND CXX_WARN_FLAGS -Wmissing-declarations)
    list (APPEND CXX_WARN_FLAGS -Wredundant-decls)
    #list (APPEND CXX_WARN_FLAGS -Winline) # warns on empty struct destructors
    list (APPEND CXX_WARN_FLAGS -Wvla)
    list (APPEND CXX_WARN_FLAGS -Wstack-protector)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8)
        list (APPEND CXX_WARN_FLAGS -Wcast-align=strict)
        list (APPEND CXX_WARN_FLAGS -Wextra-semi)
    else ()
        list (APPEND CXX_WARN_FLAGS -Wcast-align)
    endif ()
    # C flags for sha2
    list (APPEND C_WARN_FLAGS -Wall)
    list (APPEND C_WARN_FLAGS -Wextra)
    list (APPEND C_WARN_FLAGS -Wmissing-format-attribute)
    list (APPEND C_WARN_FLAGS -Wmissing-noreturn)
    list (APPEND C_WARN_FLAGS -Wpointer-arith)
    list (APPEND C_WARN_FLAGS -Wshadow)
    list (APPEND C_WARN_FLAGS -Wstrict-prototypes)
    list (APPEND C_WARN_FLAGS -Wundef)
    list (APPEND C_WARN_FLAGS -Wunused)
    list (APPEND C_WARN_FLAGS -Wwrite-strings)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list (APPEND CXX_WARN_FLAGS -Weverything)
    list (APPEND CXX_WARN_FLAGS -Wno-padded)
    list (APPEND CXX_WARN_FLAGS -Wno-c++98-compat)
    list (APPEND CXX_WARN_FLAGS -Wno-c++98-compat-pedantic)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6)
        list (APPEND CXX_WARN_FLAGS -Wno-zero-as-null-pointer-constant) # warns on system headers
        list (APPEND CXX_WARN_FLAGS -Wno-undefined-func-template) # warns on unspecialized scope_guard
        list (APPEND CXX_WARN_FLAGS -Wno-missing-variable-declarations) # warns on inline variable declarations
    endif ()
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7)
        list (APPEND CXX_WARN_FLAGS -Wno-return-std-move-in-c++11)
    endif ()
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 9)
        list (APPEND CXX_WARN_FLAGS -Wno-ctad-maybe-unsupported) # produces fp's on std::lock_guard with clang-9/-10/-11
    endif ()
    # C flags for sha2
    list (APPEND C_WARN_FLAGS -Wall)
    list (APPEND C_WARN_FLAGS -Wextra)
    list (APPEND C_WARN_FLAGS -Wmissing-format-attribute)
    list (APPEND C_WARN_FLAGS -Wmissing-noreturn)
    list (APPEND C_WARN_FLAGS -Wpointer-arith)
    list (APPEND C_WARN_FLAGS -Wshadow)
    list (APPEND C_WARN_FLAGS -Wstrict-prototypes)
    list (APPEND C_WARN_FLAGS -Wundef)
    list (APPEND C_WARN_FLAGS -Wunused)
    list (APPEND C_WARN_FLAGS -Wwrite-strings)
else ()
    message (WARNING "No warnings set for compiler ${CMAKE_CXX_COMPILER_ID}!")
endif ()
list (JOIN C_WARN_FLAGS " " CMAKE_C_FLAGS)
list (JOIN CXX_WARN_FLAGS " " CMAKE_CXX_FLAGS)


if (FORCE_COLORED_OUTPUT)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options (-fdiagnostics-color=always)
        add_link_options (-fdiagnostics-color=always)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options (-fcolor-diagnostics)
        add_link_options (-fcolor-diagnostics)
    else ()
        message (FATAL_ERROR "Do not know how to force colored output compiler ${CMAKE_CXX_COMPILER_ID}!")
    endif ()
endif ()


add_link_options (LINKER:--as-needed)


# TODO:
# Fails to find stddef.h, see https://github.com/include-what-you-use/include-what-you-use/blob/master/README.md#how-to-install
# Also fails when using gcc or with option -pie
#find_program (IWYU_EXE NAMES include-what-you-use iwyu)
#if (NOT IWYU_EXE)
#    message (NOTICE "Could not find the program include-what-you-use")
#else ()
#    message (STATUS "Found include-what-you-use: ${IWYU_EXE}")
#    set (CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_EXE};-Xiwyu;--cxx17ns;-Xiwyu;--transitive_includes_only)
#endif ()


if (NOT ENABLE_SANITIZERS)
    set (CMAKE_LINK_WHAT_YOU_USE TRUE)
endif ()


if (RUN_CLANG_TIDY)
    find_program (CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-12 clang-tidy-11 clang-tidy-10 clang-tidy-9)
    if (NOT CLANG_TIDY_EXE)
        message (FATAL_ERROR "Could not find the program clang-tidy!")
    endif()

    message (STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
    set (CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE};-checks=*,-modernize-use-trailing-return-type,-fuchsia-overloaded-operator,-readability-implicit-bool-conversion,-fuchsia-default-arguments-calls,-google-runtime-references,-hicpp-signed-bitwise,-cppcoreguidelines-avoid-magic-numbers,-readability-magic-numbers,-llvmlibc-implementation-in-namespace,-llvmlibc-callee-namespace,-llvmlibc-restrict-system-libc-headers)
endif ()


enable_testing ()


add_subdirectory (config)
add_subdirectory (docs)
add_subdirectory (itests)
add_subdirectory (rules)
add_subdirectory (src)


message (STATUS )
message (STATUS "############# Configuration for ${PROJECT_NAME} ${PROJECT_VERSION} #############")
message (STATUS )
message (STATUS "    ENABLE_DOC                ${ENABLE_DOC}")
message (STATUS "    ENABLE_HARDENING          ${ENABLE_HARDENING}")
message (STATUS "    FAIL_ON_WARNINGS          ${FAIL_ON_WARNINGS}")
message (STATUS "    ENABLE_LTO                ${ENABLE_LTO}")
message (STATUS "    FORCE_COLORED_OUTPUT      ${FORCE_COLORED_OUTPUT}")
message (STATUS "    ENABLE_SANITIZERS         ${ENABLE_SANITIZERS}")
message (STATUS "    RUN_CLANG_TIDY            ${RUN_CLANG_TIDY}")
message (STATUS )
message (STATUS "    Compiler                  ${CMAKE_CXX_COMPILER_ID}")
message (STATUS "    Build Type                ${CMAKE_BUILD_TYPE}")
message (STATUS "    Install prefix            ${CMAKE_INSTALL_PREFIX}")
get_directory_property (DirCOpts DIRECTORY ${CMAKE_SOURCE_DIR}/src/ COMPILE_OPTIONS)
string (REGEX REPLACE "(^|[^\\\\]);" "\\1 " DirCOpts "${DirCOpts}")
message (STATUS "    Compile Options           ${DirCOpts}")
message (STATUS "    C++ Flags (General)       ${CMAKE_CXX_FLAGS}")
message (STATUS "    C++ FLags (Release)       ${CMAKE_CXX_FLAGS_RELEASE}")
message (STATUS "    C++ FLags (Debug)         ${CMAKE_CXX_FLAGS_DEBUG}")
get_directory_property (DirDefs DIRECTORY ${CMAKE_SOURCE_DIR}/src/ COMPILE_DEFINITIONS)
string (REGEX REPLACE "(^|[^\\\\]);" "\\1 " DirDefs "${DirDefs}")
message (STATUS "    Defines                   ${DirDefs}")
get_directory_property (DirLOpts DIRECTORY ${CMAKE_SOURCE_DIR}/src/ LINK_OPTIONS)
string (REGEX REPLACE "(^|[^\\\\]);" "\\1 " DirLOpts "${DirLOpts}")
message (STATUS "    Link Options              ${DirLOpts}")
message (STATUS )
message (STATUS "#########################################################")
message (STATUS )

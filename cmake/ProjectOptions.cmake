# Settings applied to every target this project owns. Third-party targets are left alone.

function(tpj_configure_target target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)

    if(TPJ_SANITIZE)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()

    if(TPJ_CLANG_TIDY)
        find_program(TPJ_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
        # GCC-only warning flags in the compile line are unknown to clang.
        set_target_properties(${target} PROPERTIES
            CXX_CLANG_TIDY "${TPJ_CLANG_TIDY_EXE};--warnings-as-errors=*;--extra-arg=-Wno-unknown-warning-option")
    endif()

    # A static runtime lets the Windows binary run outside an MSYS2 shell.
    get_target_property(target_type ${target} TYPE)
    if(MINGW AND target_type STREQUAL "EXECUTABLE")
        target_link_options(${target} PRIVATE -static)
    endif()
endfunction()

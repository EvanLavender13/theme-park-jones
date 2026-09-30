# Settings applied to every target this project owns. Third-party targets are left alone.

function(tpj_configure_target target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)

    if(TPJ_SANITIZE)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()

    # A static runtime lets the Windows binary run outside an MSYS2 shell.
    get_target_property(target_type ${target} TYPE)
    if(MINGW AND target_type STREQUAL "EXECUTABLE")
        target_link_options(${target} PRIVATE -static)
    endif()
endfunction()

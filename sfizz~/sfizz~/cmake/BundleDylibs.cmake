# SPDX-License-Identifier: BSD-2-Clause

# This code is part of the sfizz library and is licensed under a BSD 2-clause
# license. You should have receive a LICENSE.md file along with the code.
# If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

# Dylib bundler for macOS
# Requires the external program "dylibbundler"

if(APPLE)
    find_program(DYLIBBUNDLER_PROGRAM "dylibbundler")
    if(NOT DYLIBBUNDLER_PROGRAM)
        message(WARNING "The installation helper \"dylibbundler\" is not available.")
    endif()
endif()

function(bundle_dylibs NAME PATH)
    if(NOT APPLE OR NOT DYLIBBUNDLER_PROGRAM)
        return()
    endif()

    set(_relative_libdir "../Frameworks")

    get_filename_component(_dir "${PATH}" DIRECTORY)
    set(_dir "${_dir}/${_relative_libdir}")

    set(_script "${CMAKE_CURRENT_BINARY_DIR}/_bundle-dylibs.${NAME}.cmake")

    file(WRITE "${_script}"
"execute_process(COMMAND \"${DYLIBBUNDLER_PROGRAM}\"
    \"-cd\" \"-of\" \"-b\"
    \"-x\" \"\$ENV{DESTDIR}${PATH}\"
    \"-d\" \"\$ENV{DESTDIR}${_dir}\"
    \"-p\" \"@loader_path/${_relative_libdir}/\")
")

    install(SCRIPT "${_script}" ${ARGN})
endfunction()

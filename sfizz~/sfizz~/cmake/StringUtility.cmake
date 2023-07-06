# SPDX-License-Identifier: BSD-2-Clause

# This code is part of the sfizz library and is licensed under a BSD 2-clause
# license. You should have receive a LICENSE.md file along with the code.
# If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

function(string_left_pad VAR INPUT LENGTH FILLCHAR)
    set(_output "${INPUT}")
    string(LENGTH "${_output}" _length)
    while(_length LESS "${LENGTH}")
        set(_output "${FILLCHAR}${_output}")
        string(LENGTH "${_output}" _length)
    endwhile()
    set("${VAR}" "${_output}" PARENT_SCOPE)
endfunction()

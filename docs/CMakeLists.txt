if (ENABLE_DOC)
    find_program (ASCIIDOCTOR_EXE asciidoctor)
    if (NOT ASCIIDOCTOR_EXE)
        message (FATAL_ERROR "Could not find the program asciidoctor!\n\tyou can skip the generation of manpages with ENABLE_DOC=OFF")
    endif ()

    if (NOT ASCIIDOCTOR_VERSION)
        execute_process (
            COMMAND ${ASCIIDOCTOR_EXE} --version
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            OUTPUT_VARIABLE _out
            )
        string (REGEX MATCH "Asciidoctor  *[0-9]+\\.[0-9]+\\.[0-9]+" _line1 "${_out}[0]")
        string (REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" _version "${_line1}")
        unset (_line1)
        unset (_out)

        message (STATUS "Found asciidoctor: ${ASCIIDOCTOR_EXE} (found version \"${_version}\")")
        set (ASCIIDOCTOR_VERSION _version CACHE STRING "Asciidoctor version")
        mark_as_advanced (ASCIIDOCTOR_VERSION)
    endif ()

    set (ASCIIDOCTOR_OPTS -b manpage -d manpage -v)
    if (ASCIIDOCTOR_VERSION VERSION_GREATER_EQUAL 2)
        LIST(APPEND ASCIIDOCTOR_OPTS -w)
    endif ()

    find_program (MAN_EXE man)
    if (NOT MAN_EXE)
        message (FATAL_ERROR "Could not find the program man!")
    endif ()

    if (NOT MAN_EXE_PRINTED)
        message (STATUS "Found man: ${MAN_EXE}")
        set (MAN_EXE_PRINTED TRUE CACHE BOOL "man found message printed")
        mark_as_advanced (MAN_EXE_PRINTED)
    endif ()


    set (MAN_FILES
        ctguard-diskscan.8
        ctguard-intervention.8
        ctguard-logscan.8
        ctguard-research.8
        diskscan.conf.5
        intervention.conf.5
        logscan.conf.5
        research.conf.5
        rules.xml.5
        )

    set (MAN_TEMPLATES footer.adoc.template)

    foreach (m IN LISTS MAN_FILES)
        set (ms ${m}.adoc)
        add_custom_command (
            OUTPUT ${m}
            COMMAND ${ASCIIDOCTOR_EXE} ${ASCIIDOCTOR_OPTS} ${ms}
            COMMAND sh -c "LC_ALL=en_US.UTF-8 MANROFFSEQ='' MANWIDTH=80 ${MAN_EXE} --warnings -E UTF-8 -l -Tutf8 -Z ${m} >/dev/null"
            MAIN_DEPENDENCY ${ms}
            DEPENDS ${MAN_TEMPLATES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Building manpage ${m}"
            VERBATIM
            )
        configure_file (${ms} ${ms} COPYONLY)
        endforeach ()
    foreach (m IN LISTS MAN_TEMPLATES)
        configure_file (${m} ${m} COPYONLY)
    endforeach ()

    add_custom_target (man
        ALL
        DEPENDS ${MAN_FILES} ${MAN_TEMPLATES}
        COMMENT "Building manpages"
        )

    install (FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ctguard-diskscan.8
        ${CMAKE_CURRENT_BINARY_DIR}/ctguard-intervention.8
        ${CMAKE_CURRENT_BINARY_DIR}/ctguard-logscan.8
        ${CMAKE_CURRENT_BINARY_DIR}/ctguard-research.8
        DESTINATION /usr/share/man/man8
        )
    install (FILES
        ${CMAKE_CURRENT_BINARY_DIR}/diskscan.conf.5
        ${CMAKE_CURRENT_BINARY_DIR}/intervention.conf.5
        ${CMAKE_CURRENT_BINARY_DIR}/logscan.conf.5
        ${CMAKE_CURRENT_BINARY_DIR}/research.conf.5
        ${CMAKE_CURRENT_BINARY_DIR}/rules.xml.5
        DESTINATION /usr/share/man/man5
        )
endif ()

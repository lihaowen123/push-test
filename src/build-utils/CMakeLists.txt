
option(SLIC3R_ENC_CHECK "Verify encoding of source files" 1)

if (IS_CROSS_COMPILE)
    # Force disable due to cross compilation. This fact is already printed on cli for users
    set(SLIC3R_ENC_CHECK OFF CACHE BOOL "" FORCE)
endif ()

if (SLIC3R_ENC_CHECK)
    # add_executable(encoding-check encoding-check.cpp)

    # # A global no-op target which depends on all encodings checks,
    # # and on which in turn all checked targets depend.
    # # This is done to make encoding checks the first thing to be
    # # performed before actually compiling any sources of the checked targets
    # # to make the check fail as early as possible.
    # add_custom_target(global-encoding-check
    #     ALL
    #     DEPENDS encoding-check
    # )
endif()

# Function that adds source file encoding check to a target
# using the above encoding-check binary

function(encoding_check TARGET)
    # if (SLIC3R_ENC_CHECK)
    #     # Obtain target source files
    #     get_target_property(T_SOURCES ${TARGET} SOURCES)

    #     # Define top-level encoding check target for this ${TARGET}
    #     add_custom_target(encoding-check-${TARGET}
    #         DEPENDS encoding-check ${T_SOURCES}
    #         COMMENT "Checking source files encodings for target ${TARGET}"
    #     )

    #     # Add checking of each source file as a subcommand of encoding-check-${TARGET}
    #     foreach(file ${T_SOURCES})
    #         add_custom_command(TARGET encoding-check-${TARGET}
    #             COMMAND $<TARGET_FILE:encoding-check> ${TARGET} ${file}
    #             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    #         )
    #     endforeach()

    #     # This adds dependency on encoding-check-${TARGET} to ${TARET}
    #     # via the global-encoding-check
    #     add_dependencies(global-encoding-check encoding-check-${TARGET})
    #     add_dependencies(${TARGET} global-encoding-check)
    # endif()
endfunction()

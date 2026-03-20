find_package(doctest CONFIG REQUIRED)

function(add_doctest)
    cmake_parse_arguments(
        DOCTEST
        ""
        "NAME"
        "SOURCES;LIBS"
        ${ARGN}
    )

    if(NOT DOCTEST_NAME)
        message(FATAL_ERROR "add_doctest_bundle: NAME is required")
    endif()

    if(NOT DOCTEST_SOURCES)
        message(FATAL_ERROR "add_doctest_bundle: SOURCES is required")
    endif()

    add_executable(${DOCTEST_NAME}
        ${DOCTEST_SOURCES}
    )

    target_compile_features(${DOCTEST_NAME}
        PRIVATE cxx_std_20
    )

    target_link_libraries(${DOCTEST_NAME}
        PRIVATE
        doctest::doctest
        ${DOCTEST_LIBS}
    )

    # Important : pour CTest
    include(doctest)

    doctest_discover_tests(${DOCTEST_NAME}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        TEST_PREFIX ${DOCTEST_NAME}/
    )
endfunction()

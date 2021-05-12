file(COPY ${CMAKE_SOURCE_DIR}/.clang-format 
        DESTINATION ${CMAKE_BINARY_DIR})

find_program(CLANG_FORMAT
	clang-format
	NAMES clang-format-13 clang-format-13 clang-format-12 clang-format-11 
          clang-format13 clang-format13 clang-format12 clang-format11 
	REQUIRED)

set(TESTS
	test_object.conf
)

foreach(TEST ${TESTS})
	get_filename_component(TEST_NAME ${TEST} NAME)
	string(REGEX REPLACE "[.]conf$" "" TEST_BIN ${TEST_NAME})
	string(REGEX REPLACE "[.]conf$" ".cc" TEST_SRC ${TEST})
	string(REGEX REPLACE "[.]conf$" ".h" TEST_HEADER ${TEST_NAME})
	string(REGEX REPLACE "[.]conf$" ".conf.expected" TEST_EXPECTED ${TEST})
	add_custom_command(OUTPUT ${TEST_HEADER}
		COMMAND config-gen "${CMAKE_CURRENT_SOURCE_DIR}/${TEST}" "-o" ${TEST_HEADER}
		COMMENT "Generating test header ${TEST_HEADER}"
		MAIN_DEPENDENCY ${TEST})
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_SRC}")
		add_executable(${TEST_BIN} ${TEST_SRC} "${CMAKE_CURRENT_BINARY_DIR}/${TEST_HEADER}")
		target_include_directories(${TEST_BIN} PRIVATE ${UCL_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR})
		target_link_libraries(${TEST_BIN} PRIVATE ${UCL_LIBRARY})
		add_test(NAME ${TEST_BIN} COMMAND ${TEST_BIN})
	endif()
endforeach()

project(
		ini-unit-test
		LANGUAGES CXX
)

file(
		GLOB_RECURSE
		${PROJECT_NAME}_SOURCE
		CONFIGURE_DEPENDS

		src/*.cpp
)

add_executable(
		${PROJECT_NAME}

		${${PROJECT_NAME}_SOURCE}
)

target_compile_definitions(
	${PROJECT_NAME}
	PUBLIC

	BOOST_UT_DISABLE_MODULE
	TEST_INI_READER_FILE_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_ini_reader.ini"
	TEST_INI_READER_WITH_COMMENT_FILE_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_ini_reader_with_comment.ini"
	TEST_INI_WRITER_FILE_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_ini_writer.ini"
	TEST_INI_WRITER_WITH_COMMENT_FILE_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_ini_writer_with_comment.ini"
)

include(${${PROJECT_NAME_PREFIX}3RD_PARTY_PATH}/ut/ut.cmake)

target_link_libraries(
		${PROJECT_NAME}
		PRIVATE
		gal::ini
)

add_test(
		NAME ${PROJECT_NAME}
		COMMAND ${PROJECT_NAME}
)

project(
		GAL-INI-unit-test
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

CPM_link_libraries_DECL()
include(${${PROJECT_NAME_PREFIX}CMAKE_3RDPARTY_PATH}/boost-ext-ut.cmake)
CPM_link_libraries_LINK(${PROJECT_NAME})

target_link_libraries(
		${PROJECT_NAME}
		PRIVATE
#		gal::ini-shared
		gal::ini-static
)

set_compile_property(${PROJECT_NAME})
turn_off_warning(${PROJECT_NAME})

add_test(
		NAME ${PROJECT_NAME}
		COMMAND ${PROJECT_NAME}
)

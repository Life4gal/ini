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
)

set_compile_options_private(${PROJECT_NAME})
turn_off_warning(${PROJECT_NAME})

CPM_link_libraries_DECL()
include(${${PROJECT_NAME_PREFIX}CMAKE_3RDPARTY_PATH}/boost-ext-ut.cmake)
CPM_link_libraries_LINK(${PROJECT_NAME})

target_link_libraries(
		${PROJECT_NAME}
		PRIVATE
#		gal::ini-shared
		gal::ini-static
)

# cmake try to find 'bin\CMakeTemplateProject-impl-shared.lib' but it not exist if did not exported any symbol...
#get_target_property(CTP-shared-original-name gal::CTP-shared ALIASED_TARGET)
#add_library(CTP-shared SHARED IMPORTED)
#set_target_properties(
#		CTP-shared PROPERTIES
##		IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/${CTP-shared-original-name}.dll
#		IMPORTED_LOCATION ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CTP-shared-original-name}.dll
##		IMPORTED_IMPLIB ${CMAKE_BINARY_DIR}/${CTP-shared-original-name}.lib
#		IMPORTED_IMPLIB ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CTP-shared-original-name}.lib
#)
#target_link_libraries(
#		${PROJECT_NAME}
#		PRIVATE
#		CTP-shared
#)

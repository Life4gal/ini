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
	TEST_INI_EXTRACTOR_FILE_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_ini_extractor.ini"
	TEST_INI_FLUSHER_FILE_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_ini_flusher.ini"
)

include(${${PROJECT_NAME_PREFIX}3RD_PARTY_PATH}/ut/ut.cmake)

target_link_libraries(
		${PROJECT_NAME}
		PRIVATE
		gal::ini
)

set(
		UT_WARNINGS

		-Wno-global-constructors
)

target_compile_options(
		${PROJECT_NAME}
		PRIVATE

		# for ut
		# clang-cl
		$<$<AND:$<CXX_COMPILER_ID:Clang>,$<STREQUAL:"${CMAKE_CXX_SIMULATE_ID}","MSVC">>:${UT_WARNINGS}>
)

add_test(
		NAME ${PROJECT_NAME}
		COMMAND ${PROJECT_NAME}
)

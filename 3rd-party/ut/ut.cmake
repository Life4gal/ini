CPMAddPackage(
		NAME ut
		GIT_TAG v1.1.9
		GITHUB_REPOSITORY "boost-ext/ut"
		OPTIONS "BOOST_UT_USE_WARNINGS_AS_ERORS ON"
)

cmake_language(
		CALL
		${PROJECT_NAME_PREFIX}cpm_install
		${PROJECT_NAME} ut PRIVATE
)

#include <map>
#include <string>

#include "test_extractor_generate_file.hpp"

using namespace boost::ut;
using namespace gal::ini;

#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
#define GAL_INI_NO_DESTROY [[clang::no_destroy]]
#else
#define GAL_INI_NO_DESTROY
#endif

namespace
{
	using group_type = std::map<std::string, std::string, std::less<>>;
	using context_type = std::map<std::string, group_type, std::less<>>;

	GAL_INI_NO_DESTROY [[maybe_unused]] suite suite_generate_file = [] { do_generate_file(); };

	GAL_INI_NO_DESTROY [[maybe_unused]] suite suite_extract_from_file = [] { do_test_extract_from_file<context_type>(); };

	GAL_INI_NO_DESTROY [[maybe_unused]] suite suite_extract_from_buffer = [] { do_test_extract_from_buffer<context_type>(); };
};

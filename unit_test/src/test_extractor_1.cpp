#include <boost/ut.hpp>
#include <filesystem>
#include <fstream>
#include <ini/extractor.hpp>
#include <string>
#include <unordered_map>

using namespace boost::ut;
using namespace gal::ini;

#define GROUP1_NAME "group1"
#define GROUP2_NAME "group2"
#define GROUP3_NAME "group3 !#@#*%$^&"
#define GROUP4_NAME "group4 }{}{}{}{}{}{()()()())[[[[[[["
#define GROUP5_NAME "group5 LKGP&ITIG&PG"

#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
	#define GAL_INI_NO_DESTROY [[clang::no_destroy]]
#else
	#define GAL_INI_NO_DESTROY
#endif

namespace
{
	struct string_hasher
	{
		using is_transparent = int;

		template<typename String>
		[[nodiscard]] constexpr auto operator()(const String& string) const noexcept -> std::size_t
		{
			if constexpr (std::is_array_v<String>)
			{
				return std::hash<std::basic_string_view<typename std::pointer_traits<std::decay_t<String>>::element_type>>{}(string);
			}
			else if constexpr (std::is_pointer_v<String>)
			{
				return std::hash<std::basic_string_view<typename std::pointer_traits<String>::element_type>>{}(string);
			}
			else if constexpr (requires { std::hash<String>{}; })
			{
				return std::hash<String>{}(string);
			}
			else
			{
				[]<bool always_false = false> { static_assert(always_false, "Unsupported hash type!"); }
				();
			}
		}
	};

	using group_type														   = std::unordered_map<std::string, std::string, string_hasher, std::equal_to<>>;
	using context_type														   = std::unordered_map<std::string, group_type, string_hasher, std::equal_to<>>;

	GAL_INI_NO_DESTROY [[maybe_unused]] suite test_ini_extractor_generate_file = []
	{
		const std::filesystem::path file_path{TEST_INI_EXTRACTOR_FILE_PATH};

		std::ofstream				file{file_path, std::ios::out | std::ios::trunc};

		file << "[" GROUP1_NAME << "]\n";
		file << "key1=value1\n";
		file << "key2 =value2\n";
		file << "key3 = value3\n";
		file << " key4  =       value4\n";
		file << "\n";

		file << "; this comment will be ignored1\n";
		file << "[" GROUP2_NAME "]# this comment will be ignored2\n";
		file << "key1       =           value1\n";
		file << "       key2=value2\n";
		file << "\n";

		file << "[" GROUP3_NAME "]\n";
		file << "   =       invalid line, ignore me\n";
		file << "key1=value1\n";
		file << " !@#$%^&*()_+ ignore me \n";
		file << "key2=value2\n";
		file << "ignore me\n";

		file << "[" GROUP4_NAME "]\n";

		file << "[" GROUP5_NAME "]\n";

		file.close();
	};

	auto check_extract_result = [](const ExtractResult extract_result, const context_type& data) -> void
	{
		"extract_ok"_test = [extract_result]
		{
			expect((extract_result == ExtractResult::SUCCESS) >> fatal);
		};

		"group_size"_test = [&]
		{ expect((data.size() == 5_i) >> fatal); };

		"group_name"_test = [&]
		{
			expect(data.contains(GROUP1_NAME) >> fatal);
			expect(data.contains(GROUP2_NAME) >> fatal);
			expect(data.contains(GROUP3_NAME) >> fatal);
			expect(data.contains(GROUP4_NAME) >> fatal);
			expect(data.contains(GROUP5_NAME) >> fatal);
		};

		"group1"_test = [&]
		{
			const auto [name, group] = *data.find(GROUP1_NAME);

			expect((name == GROUP1_NAME) >> fatal);
			expect((group.size() == 4_i) >> fatal);

			expect(group.contains("key1") >> fatal);
			expect(group.contains("key2") >> fatal);
			expect(group.contains("key3") >> fatal);
			expect(group.contains("key4") >> fatal);

			expect((group.at("key1") == "value1") >> fatal);
			expect((group.at("key2") == "value2") >> fatal);
			expect((group.at("key3") == "value3") >> fatal);
			expect((group.at("key4") == "value4") >> fatal);
		};

		"group2"_test = [&]
		{
			const auto [name, group] = *data.find(GROUP2_NAME);

			expect((name == GROUP2_NAME) >> fatal);
			expect((group.size() == 2_i) >> fatal);

			expect(group.contains("key1") >> fatal);
			expect(group.contains("key2") >> fatal);

			expect((group.at("key1") == "value1") >> fatal);
			expect((group.at("key2") == "value2") >> fatal);
		};

		"group3"_test = [&]
		{
			const auto [name, group] = *data.find(GROUP3_NAME);

			expect((name == GROUP3_NAME) >> fatal);
			expect((group.size() == 2_i) >> fatal);

			expect(group.contains("key1") >> fatal);
			expect(group.contains("key2") >> fatal);

			expect((group.at("key1") == "value1") >> fatal);
			expect((group.at("key2") == "value2") >> fatal);
		};

		"group4"_test = [&]
		{
			const auto [name, group] = *data.find(GROUP4_NAME);

			expect((name == GROUP4_NAME) >> fatal);
			expect(group.empty() >> fatal);
			expect((group.size() == 0_i) >> fatal);
		};

		"group5"_test = [&]
		{
			const auto [name, group] = *data.find(GROUP5_NAME);

			expect((name == GROUP5_NAME) >> fatal);
			expect(group.empty() >> fatal);
			expect((group.size() == 0_i) >> fatal);
		};
	};

	GAL_INI_NO_DESTROY [[maybe_unused]] suite test_ini_extractor_extract_from_file = []
	{
#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
		auto  workaround_extract_result_data = extract_from_file<context_type>(TEST_INI_EXTRACTOR_FILE_PATH);
		auto& extract_result				 = workaround_extract_result_data.first;
		auto& data							 = workaround_extract_result_data.second;
#else
		auto [extract_result, data] = extract_from_file<context_type>(TEST_INI_EXTRACTOR_FILE_PATH);
#endif

		check_extract_result(extract_result, data);
	};

	GAL_INI_NO_DESTROY [[maybe_unused]] suite test_ini_extractor_extract_from_buffer = []
	{
		std::ifstream file{TEST_INI_EXTRACTOR_FILE_PATH, std::ios::in};
		expect((file.is_open()) >> fatal);

		std::string buffer{};

		file.seekg(0, std::ios::end);
		buffer.reserve(file.tellg());
		file.seekg(0, std::ios::beg);

		buffer.assign(
				std::istreambuf_iterator<char>(file),
				std::istreambuf_iterator<char>());

#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
		auto  workaround_extract_result_data = extract_from_buffer<context_type>(buffer);
		auto& extract_result				 = workaround_extract_result_data.first;
		auto& data							 = workaround_extract_result_data.second;
#else
		auto [extract_result, data] = extract_from_buffer<context_type>(buffer);
#endif

		check_extract_result(extract_result, data);
	};
}// namespace

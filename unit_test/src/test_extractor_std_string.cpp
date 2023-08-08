#include <boost/ut.hpp>
#include <filesystem>
#include <fstream>
#include <ini/extractor.hpp>
#include <map>
#include <string>

using namespace boost::ut;
using namespace gal::ini;

#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
	#define GAL_INI_NO_DESTROY [[clang::no_destroy]]
#else
#define GAL_INI_NO_DESTROY
#endif

#define GROUP1_NAME "basic test"

#define GROUP2_NAME "with whitespace"
#define GROUP2_KEY "k e y"
#define GROUP2_VALUE "v a l u e"

#define GROUP3_NAME "invalid line"

namespace
{
	using group_type = std::map<std::string, std::string, std::less<>>;
	using context_type = std::map<std::string, group_type, std::less<>>;

	GAL_INI_NO_DESTROY [[maybe_unused]] suite suite_generate_file = []
	{
		const std::filesystem::path file_path{TEST_INI_EXTRACTOR_FILE_PATH};

		std::ofstream file{file_path, std::ios::out | std::ios::trunc};

		file << '#' << GROUP1_NAME << '\n';
		file << "[" GROUP1_NAME << "]\n";
		file << '#' << "kv" << '\n';
		file << "key1 = value1\n";
		file << '#' << "kv without whitespace" << '\n';
		file << "key2=value2\n";
		file << '#' << "kv with whitespaces" << '\n';
		file << "     key3   =    value3    \n";
		file << '#' << "kv with whitespace and tab" << '\n';
		file << "   key4   =      value4\n";
		file << "\n";

		file << '#' << GROUP2_NAME << '\n';
		file << "[" GROUP2_NAME << "]\n";
		file << '#' << "kv" << '\n';
		file << GROUP2_KEY "1 = " GROUP2_VALUE "1\n";
		file << '#' << "kv without whitespace" << '\n';
		file << GROUP2_KEY "2=" GROUP2_VALUE "2\n";
		file << '#' << "kv with whitespaces" << '\n';
		file << "     " GROUP2_KEY "3   =    " GROUP2_VALUE "3    \n";
		file << '#' << "kv with whitespace and tab" << '\n';
		file << "   " GROUP2_KEY "4   =       " GROUP2_VALUE "4\n";
		file << "\n";

		file << '#' << GROUP3_NAME << '\n';


		file.close();
	};

	auto do_check_extract_result = [](const ExtractResult extract_result, const context_type& data) -> void
	{
		"extract_ok"_test = [extract_result] { expect((extract_result == ExtractResult::SUCCESS) >> fatal); };

		"group_size"_test = [&] { expect((data.size() == 2_i) >> fatal); };

		"group_name"_test = [&]
		{
			expect(data.contains(GROUP1_NAME) >> fatal);
			expect(data.contains(GROUP2_NAME) >> fatal);
		};

		"group1"_test = [&]
		{
			const auto& group = data.at(GROUP1_NAME);

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
			const auto& group = data.at(GROUP2_NAME);

			expect((group.size() == 4_i) >> fatal);

			expect(group.contains(GROUP2_KEY"1") >> fatal);
			expect(group.contains(GROUP2_KEY"2") >> fatal);
			expect(group.contains(GROUP2_KEY"3") >> fatal);
			expect(group.contains(GROUP2_KEY"4") >> fatal);

			expect((group.at(GROUP2_KEY"1") == GROUP2_VALUE"1") >> fatal);
			expect((group.at(GROUP2_KEY"2") == GROUP2_VALUE"2") >> fatal);
			expect((group.at(GROUP2_KEY"3") == GROUP2_VALUE"3") >> fatal);
			expect((group.at(GROUP2_KEY"4") == GROUP2_VALUE"4") >> fatal);
		};
	};

	GAL_INI_NO_DESTROY [[maybe_unused]] suite suite_extract_from_file = []
	{
		#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
			auto  workaround_extract_result_data = extract_from_file<context_type>(TEST_INI_EXTRACTOR_FILE_PATH);
			auto& extract_result				 = workaround_extract_result_data.first;
			auto& data							 = workaround_extract_result_data.second;
		#else
		auto [extract_result, data] = extract_from_file<context_type>(TEST_INI_EXTRACTOR_FILE_PATH);
		#endif

		do_check_extract_result(extract_result, data);
	};

	GAL_INI_NO_DESTROY [[maybe_unused]] suite suite_extract_from_buffer = []
	{
		std::ifstream file{TEST_INI_EXTRACTOR_FILE_PATH, std::ios::in};
		expect((file.is_open()) >> fatal);

		std::string buffer{};

		file.seekg(0, std::ios::end);
		buffer.reserve(static_cast<std::string::size_type>(file.tellg()));
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

		do_check_extract_result(extract_result, data);
	};
};

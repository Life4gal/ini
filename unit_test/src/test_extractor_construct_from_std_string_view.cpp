#include <boost/ut.hpp>
#include <filesystem>
#include <fstream>
#include <ini/extractor.hpp>
#include <map>
#include <string>

using namespace boost::ut;
using namespace gal::ini;
using namespace std::string_view_literals;

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
	class UserString
	{
	public:
		using string_type = std::string;
		using string_view_type = std::string_view;

		using value_type = string_type::value_type;

	private:
		string_type string_;

	public:
		explicit(false) UserString(const string_view_type string)
			: string_{string} {}

		explicit(false) UserString(const char* string)
			: string_{string} {}

		[[nodiscard]] auto data() const noexcept -> const string_type& { return string_; }

		friend auto operator<=>(const UserString& lhs, const UserString& rhs) noexcept -> auto { return lhs.data() <=> rhs.data(); }

		friend auto operator<=>(const UserString& lhs, const string_type& rhs) noexcept -> auto { return lhs.data() <=> rhs; }

		friend auto operator<=>(const string_type& lhs, const UserString& rhs) noexcept -> auto { return lhs <=> rhs.data(); }

		friend auto operator<=>(const UserString& lhs, const string_view_type rhs) noexcept -> auto { return lhs.data() <=> rhs; }

		friend auto operator<=>(const string_view_type lhs, const UserString& rhs) noexcept -> auto { return lhs <=> rhs.data(); }

		friend auto operator<=>(const UserString& lhs, const string_type::pointer rhs) noexcept -> auto { return lhs.data() <=> rhs; }

		friend auto operator<=>(const string_type::pointer lhs, const UserString& rhs) noexcept -> auto { return lhs <=> rhs.data(); }

		template<std::size_t N>
		friend auto operator<=>(const UserString& lhs, const string_type::value_type (&rhs)[N]) noexcept -> auto { return lhs.data() <=> rhs; }

		template<std::size_t N>
		friend auto operator<=>(const string_type::value_type (&lhs)[N], const UserString& rhs) noexcept -> auto { return lhs <=> rhs.data(); }

		[[nodiscard]] explicit(false) operator string_view_type() const noexcept { return string_; }
	};

	static_assert(not appender_traits<UserString>::allocatable);

	using group_type = std::map<UserString, UserString, std::less<>>;
	using context_type = std::map<UserString, group_type, std::less<>>;

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

			expect(group.contains(GROUP2_KEY "1") >> fatal);
			expect(group.contains(GROUP2_KEY "2") >> fatal);
			expect(group.contains(GROUP2_KEY "3") >> fatal);
			expect(group.contains(GROUP2_KEY "4") >> fatal);

			expect((group.at(GROUP2_KEY "1") == GROUP2_VALUE "1") >> fatal);
			expect((group.at(GROUP2_KEY "2") == GROUP2_VALUE "2") >> fatal);
			expect((group.at(GROUP2_KEY "3") == GROUP2_VALUE "3") >> fatal);
			expect((group.at(GROUP2_KEY "4") == GROUP2_VALUE "4") >> fatal);
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
}// namespace

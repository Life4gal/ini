#include <boost/ut.hpp>
#include <filesystem>
#include <fstream>
#include <ini/ini.hpp>

using namespace boost::ut;
using namespace gal::ini;

#define GROUP1_NAME "group1"
#define GROUP2_NAME "group2"
#define GROUP3_NAME "group3 !#@#*%$^&"
#define GROUP4_NAME "group4 }{}{}{}{}{}{()()()())[[[[[[["
#define GROUP5_NAME "group5 LKGP&ITIG&PG"

suite generate_file = []
{
	const std::filesystem::path file_path{TEST_INI_READER_FILE_PATH};

	std::ofstream				file{file_path, std::ios::out | std::ios::trunc};

	file << "[" GROUP1_NAME
			"]\n"
			"key1=value1\n"
			"key2 =value2\n"
			"key3 = value3\n"
			" key4  =       value4\n"
			"\n"
			"; this comment will be ignored1\n"
			"[" GROUP2_NAME
			"]# this comment will be ignored2\n"
			"key1       =           value1\n"
			"       key2=value2\n"
			"\n"
			"[" GROUP3_NAME
			"]\n"
			"[" GROUP4_NAME
			"]\n"
			"[" GROUP5_NAME "]\n";

	file.close();
};

suite test_group = []
{
	IniReader reader{TEST_INI_READER_FILE_PATH};

	"group_size"_test = [&]
	{
		expect((reader.size() == 5_i) >> fatal);
	};

	"group_name"_test = [&]
	{
		expect(reader.contains(GROUP1_NAME) >> fatal);
		expect(reader.contains(GROUP2_NAME) >> fatal);
		expect(reader.contains(GROUP3_NAME) >> fatal);
		expect(reader.contains(GROUP4_NAME) >> fatal);
		expect(reader.contains(GROUP5_NAME) >> fatal);
	};

	"group2"_test = [&]
	{
		const auto r = reader.read(string_view_type{GROUP2_NAME});

		expect((r.name() == GROUP2_NAME) >> fatal);
		expect((r.size() == 2_i) >> fatal);

		expect(r.contains("key1") >> fatal);
		expect(r.contains("key2") >> fatal);

		expect((r.get("key1") == "value1") >> fatal);
		expect((r.get("key2") == "value2") >> fatal);
	};

	"group3"_test = [&]
	{
		const auto r = reader.read(string_view_type{GROUP3_NAME});

		expect((r.name() == GROUP3_NAME) >> fatal);
		expect(r.empty() >> fatal);
		expect((r.size() == 0_i) >> fatal);
	};

	"group4"_test = [&]
	{
		const auto r = reader.read(string_view_type{GROUP4_NAME});

		expect((r.name() == GROUP4_NAME) >> fatal);
		expect(r.empty() >> fatal);
		expect((r.size() == 0_i) >> fatal);
	};

	"group5"_test = [&]
	{
		const auto r = reader.read(string_view_type{GROUP5_NAME});

		expect((r.name() == GROUP5_NAME) >> fatal);
		expect(r.empty() >> fatal);
		expect((r.size() == 0_i) >> fatal);
	};
};

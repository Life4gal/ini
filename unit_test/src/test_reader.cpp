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

suite test_group = []
{
	IniParser reader{TEST_INI_READER_FILE_PATH};

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

	"group1"_test = [&]
	{
		const auto r = reader.read(string_view_type{GROUP1_NAME});

		expect((r.name() == GROUP1_NAME) >> fatal);
		expect((r.size() == 4_i) >> fatal);

		expect(r.contains("key1") >> fatal);
		expect(r.contains("key2") >> fatal);
		expect(r.contains("key3") >> fatal);
		expect(r.contains("key4") >> fatal);

		expect((r.get("key1") == "value1") >> fatal);
		expect((r.get("key2") == "value2") >> fatal);
		expect((r.get("key3") == "value3") >> fatal);
		expect((r.get("key4") == "value4") >> fatal);
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
		expect((r.size() == 2_i) >> fatal);

		expect(r.contains("key1") >> fatal);
		expect(r.contains("key2") >> fatal);

		expect((r.get("key1") == "value1") >> fatal);
		expect((r.get("key2") == "value2") >> fatal);
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
